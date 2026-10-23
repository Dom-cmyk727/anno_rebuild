// Simulation configuration (spec: docs/sim/state.md §3).
//
// Every original count, size and period is data with the original value as
// its default (PLAN §0.1): SimConfig (sizes, divisors, periods), Rules (the
// opt-in changes of §13, every default is the original's behaviour) and
// ListLayout (which building list a PRODTYPE belongs to, in which order the
// lists are scheduled and searched). All three are validated before any
// state is built from them (§3.1) and are hashed in the header scope
// (state_fields.hpp), so two peers with different options diverge at tick 0.
// The free-slot policy enum and the sanity bounds come first because the slot
// list (S2) needs only them; SimConfig::validate calls the predicates instead
// of repeating their bounds.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/ids.hpp>
#include <anno/sim/production.hpp>

namespace anno::sim {

/// Sanity bound for SimConfig::list_min_growth and SimConfig::list_initial_size
/// (SimConfig::validate checks both against it). Reason: both reach
/// `vector::resize` on the first insert, so a hostile config or save with
/// 0xFFFFFFFF would ask for about 4 G elements (bad_alloc, which sim code must
/// not throw, or an OOM kill). 2^24 slots is far above any list a game builds
/// (the original's lists stop at 32 768, docs/sim/state.md §5.1). Path to
/// raise it: change this constant. It is a build-time bound and deliberately
/// not a SimConfig field: a bound that is read from the very header it
/// protects against could be set by a hostile header (docs/sim/state.md §15,
/// S3). It is never a type limit (slot ids are u32, see SlotList::grown_size).
inline constexpr std::uint32_t kListSizeBound = 1U << 24;

/// The most memory the slots of ONE list may take when a state is read (F35). A slot
/// is one byte on the wire when empty but a whole std::optional<T> in memory (about
/// 70 bytes for a Figure), so a hostile 16 MB stream of empty slots would make the
/// reader allocate over a gigabyte; the count is therefore also bounded by this
/// budget (state_fields.hpp, slot_list_fits_load_budget). A list the configuration
/// allows but whose slots take more than the budget (2^24 slots of a Figure) can be
/// saved but not loaded. Path to make it configurable: a load option that replaces
/// this constant; like kListSizeBound it is not a header field.
inline constexpr std::uint64_t kSlotListLoadBytes = std::uint64_t{256} << 20;

/// How many UI events a world keeps for the host between two drain_events() calls
/// unless the host sets another capacity (World::set_event_capacity). Reason: a
/// headless host, a test, a replay or a bot that never drains must not grow the queue
/// without bound over a long game (every blocked/status/notice/spawn event adds an
/// entry, and every copy of a World, for example the net thread's checksum snapshot,
/// copies the queue). Not state; the per-advance list SimOutput::events is unbounded
/// by one advance only.
inline constexpr std::uint64_t kHostEventCapacityDefault = std::uint64_t{1} << 16;

/// Sanity bound for the slot tables SimConfig::island_slots, city_slots and
/// player_slots (SimConfig::validate checks them against it). Reason: the
/// loader sizes these tables, and the per-island and per-city containers that
/// hang off them, from the config alone (no byte of the stream backs them), so
/// a hostile header with 0xFFFFFFFF would ask for gigabytes (bad_alloc, which
/// sim code must not throw, or an OOM kill). 2^16 is far above any game (the
/// original has 256 islands, 16 cities per island, 32 players). Path to raise
/// it: change this constant; like kListSizeBound it is a build-time bound and
/// deliberately not a header field (docs/sim/state.md §3.1, F25).
inline constexpr std::uint32_t kSlotTableBound = 1U << 16;

/// Sanity bound for the small per-city and per-player tables: script_timers,
/// monopoly_entries, house_groups, research_entries, trade_lists,
/// warehouse_goods and build_goods_count. Reason as kSlotTableBound; these are
/// multiplied by the number of cities or players the stream holds, so the bound
/// is tighter. 2^12 is far above the originals (5 timers, 28 monopoly entries,
/// 5 house groups, 65 research entries, 2 trade lists, 56 warehouse goods, 5
/// build goods). It also bounds the null slots one city starts with, the number of
/// lists times SimConfig::list_initial_size (the original has 6 x 1): a city is
/// made from the config alone, in every city of a scenario, on worker threads.
/// Path to raise it: change this constant.
inline constexpr std::uint32_t kEntryTableBound = 1U << 12;

/// Sanity bound for a world edge in tiles (WorldSettings::world_width and
/// world_height) and for SimConfig::chunk_edge. Reason: the island lookup grid
/// of the world and the chunk grid of an island are derived from the
/// dimensions (ceil(edge / chunk_edge) cells per axis) without stream backing,
/// so an unbounded edge is an unbounded allocation, and `w + chunk_edge - 1`
/// must not wrap. The original's worlds are a few thousand tiles wide. Path to
/// raise it: change this constant (the products are formed in u64).
inline constexpr std::uint32_t kWorldEdgeBound = 1U << 14;

/// Sanity bound for the number of tiles of ONE island (TileLayer::reset refuses
/// more, the world builder and the loader check it before sizing anything).
/// Reason: a tile costs 13 bytes in the columns (docs/sim/state.md §9.2), so an
/// island of w · h tiles with w and h up to kWorldEdgeBound (2^14) would ask for
/// 3.5 GB from a single field of a hostile header. 2^26 tiles (8192 x 8192, about
/// 870 MB of columns) is far above the originals (islands a few hundred tiles
/// wide). It bounds one island, not the sum: what a whole scenario or save
/// allocates is bounded by its input (the loader checks w · h · 13 against the
/// bytes that remain) or, for World::create, by the world's own area, because the
/// rectangles are checked inside the world and disjoint BEFORE any tile is
/// allocated (validate_island_rects). The host that builds worlds from untrusted
/// scenarios must still expect `std::bad_alloc` on a host that has less memory
/// than the scenario's world needs. Path to raise it: change this constant; like
/// the other bounds it is a build-time constant and not a header field
/// (docs/sim/state.md §3.1, F27, F30). Raising it to 2^32 or more also needs
/// wider tile indices (Slot, ScenarioSpec::TileObjectSpec::index), see the
/// static_assert below.
inline constexpr std::uint64_t kIslandTileBound = 1ULL << 26;
static_assert(kIslandTileBound < kNoSlot,
              "a tile index is a Slot (u32) and kNoSlot must stay unreachable: widen Slot, "
              "TileObjectSpec::index and the ScenarioSpec fields before raising the bound");

/// Whether a w x h island is within kIslandTileBound tiles: the one place the
/// bound is compared (the layer, the scenario check, the validator and the
/// stream reader all call it, so the edge is tested once: exactly the bound is
/// accepted, one tile more is not). The product of two u32 edges fits u64.
[[nodiscard]] constexpr bool island_tiles_within_bound(std::uint32_t w, std::uint32_t h) {
    return std::uint64_t{w} * h <= kIslandTileBound;
}

/// The largest value of the saved Drought `step` and `phase` (u8): the drought
/// gates compare them with drought_steps · chunk_edge and drought_phases, so a
/// larger value could never be reached (docs/sim/state.md §5.8, §9.3). Path to
/// raise it: widen the saved fields (stage S13) and this constant together.
inline constexpr std::uint32_t kMaxDroughtStep = 0xff;
/// The largest value of the saved Drought `counter` (u16), compared with
/// drought_plateau_count. Path to raise it: as kMaxDroughtStep.
inline constexpr std::uint32_t kMaxDroughtCounter = 0xffff;
/// The largest drought target ring, drought_radius / chunk_edge: the saved ring is an
/// i32 like the original's dword (signed tests at 0x1008b530..0x1008b56e) and runs from
/// 0 to the target. Path to raise it: widen Drought::ring to i64 with this constant.
inline constexpr std::uint32_t kMaxDroughtRing = 0x7fffffff;

/// SimConfig::list_min_growth (and figure_min_growth) is valid when it is in
/// 1..kListSizeBound (zero would stall the growth rule of SlotList::insert, F22).
constexpr bool valid_list_min_growth(std::uint32_t v) {
    return v >= 1 && v <= kListSizeBound;
}

/// SimConfig::list_initial_size is valid when it is in 1..kListSizeBound (at
/// least one element: check_invariants requires n >= 1, A7). SimConfig::validate
/// bounds it further by the number of lists (kEntryTableBound for the slots of one
/// city): this predicate is the bound of one list.
constexpr bool valid_list_initial_size(std::uint32_t v) {
    return v >= 1 && v <= kListSizeBound;
}

/// The largest period a 16-bit stamp difference can reach: a gate that
/// compares `(u16)(t - stamp) >= period` with a larger period never opens, so
/// SimConfig::validate rejects every such period (docs/sim/state.md §3.1).
inline constexpr std::uint32_t kMaxStampPeriod = 0xffff;

/// Default of the per-call dispatch cap of pop_due (Rules::events_max_dispatch,
/// hashed). Reason: with Rules::events_drain_new a handler that always
/// enqueues a delay-0 event would keep the loop running forever; 2^20 events
/// in one advance is far above anything a game produces (the queue holds a few
/// hundred). Reaching the cap is reported, not an error that stops the world.
inline constexpr std::uint64_t kEventsMaxDispatchDefault = 1ULL << 20;

/// Which null slot of a building list a new element takes (decision D1, filed
/// as OQ-036; docs/sim/state.md §5.2). `Original` is the faithful default.
enum class SlotPolicy : std::uint8_t {
    /// The first null slot at or after (last stored slot + 1), wrapping round
    /// (D: AnnoWelt.dll 0x10026a00): a freed slot is reused only when this scan
    /// reaches it.
    Original,
    /// The lowest-numbered null slot (opt-in; the list still never compacts).
    LowestFree,
};

/// Whether `p` is one of the enumerators (a value cast from a save or a config
/// file may be anything; Rules::validate rejects the others, so the policy test in
/// SlotList::insert never sees one; any other value is read as Original there).
constexpr bool valid_slot_policy(SlotPolicy p) {
    return p == SlotPolicy::Original || p == SlotPolicy::LowestFree;
}

/// The order rect queries and FindNext return (decision D2, OQ-037).
enum class SpatialOrder : std::uint8_t {
    /// Canonical (list in ListLayout::rect_order, y, x, id).
    YXSlot,
    /// The original's lazy unstable quicksort on y (D: 0x10027000) with
    /// swap-remove. Reserved: not implemented (O-7); Rules::validate rejects it.
    Original0x10027000,
};

/// Whether the world PRNG is one stream or one stream per island (decision
/// D11, OQ-046).
enum class PrngMode : std::uint8_t {
    /// One world stream consumed in island order (D: 0x100a1740).
    Global,
    /// Design-only: one stream per island over the shared table. Reserved:
    /// there is no per-island stream state to hash or save until stage S16, so
    /// Rules::validate rejects it.
    PerIsland,
};

/// The order of the island templates a scenario's links are resolved from
/// (docs/re/annoscene.md §5 step 1; OQ-150).
enum class IslandCandidateOrder : std::uint8_t {
    /// Bytewise by the upper-cased path below Scenes/Islands ("NORD/N_128X128_…").
    /// The order of the list the original builds into AnnoScene.dll for network
    /// games (D 0x10008187–0x100082ad: 428 entries in this order, P), and of a
    /// single-player directory scan on NTFS; the same on every machine.
    Sorted,
    /// The single-player original's InselCache.txt order, i.e. the directory
    /// enumeration of the machine it was written on (D 0x10007c50). Reserved:
    /// no cache is read, so Rules::validate rejects it.
    Cached,
};

/// The tables of the island-link selection (docs/re/annoscene.md §5, D AnnoScene
/// 0x10009550 and its callees 0x10009170, 0x10009330, 0x10009010, 0x100090b0,
/// 0x10009130). They are read when a scenario's links are resolved, before the
/// world exists. A width rank table holds `rounds` rows of one width per rank
/// position (row r, position p at r · positions + p, positions = the length of the
/// score list).
struct LinkTables {
    /// Raw-material kinds: the loop `cmp esi, 0x14` (0x10009793) over the 20 u16 of
    /// this+0x51DC and the record's 20 marks (+0x112). At most 32: a kind is a shift
    /// count into the climate's u32 mask (0x10009019).
    std::uint32_t raw_kinds = 20;
    /// Ore kinds: the loop `cmp esi, 9` (0x100097fa), the record's 9 marks (+0x13A),
    /// the exclusion test's 9 columns (0x10009158) and the capacity sum (0x100098b1).
    std::uint32_t ore_kinds = 9;
    /// Rounds of the raw and the ore distribution (0x10009799, 0x10009800).
    std::uint32_t rounds = 25;
    /// The best-score list of one pick (0x100092bc, 0x100094e1).
    std::uint32_t best_list_cap = 5;
    /// The candidate list of a link with special and normal candidates (0x10009b64).
    std::uint32_t pick_list_cap = 50;
    /// Per climate the raw kinds it takes, `1 << kind` (0x10009010, jump table
    /// 0x1000908c; a climate past the table takes none). The same values as
    /// SimConfig::monopoly_climate_mask (AnnoWelt 0x1009d2a0), held apart because the
    /// two modules hold them apart.
    std::vector<std::uint32_t> raw_climate_mask = {0x18000, 0xf009, 0xf949, 0xf06b,
                                                   0xf6b9,  0xf04d, 0xf04d};
    /// Per climate the ore bits (0x100090b0, jump table 0x10009114). The original
    /// tests a byte (`test al, imm8`), so a mask is at most 0xFF.
    std::vector<std::uint32_t> ore_climate_mask = {0x00, 0x05, 0x25, 0x01, 0x03, 0x0b, 0x0b};
    /// The bit an ore kind tests in ore_climate_mask (0x100090b4: kind 6 tests bit 0).
    std::vector<std::uint32_t> ore_mask_bit = {0, 1, 2, 3, 4, 5, 0, 7, 8};
    /// ore_kinds × ore_kinds, row = the kind to place, column = a kind the link
    /// already has: 1 = that link is not eligible (0x10009130: the byte at
    /// 0x1003AF40 + 7 · kind + e is 0xFF; docs/re/annoscene.md §5 step 3).
    std::vector<std::uint8_t> ore_exclusion = original_ore_exclusion();
    /// The widths by rank of the raw distribution (0x1003AF74, 0x10009210).
    std::vector<std::uint32_t> raw_width_rank = original_raw_width_rank();
    /// The widths by rank of the ore distribution (0x1003B144, 0x10009400).
    std::vector<std::uint32_t> ore_width_rank = original_ore_width_rank();
    /// The score of a width at rank p: 4 − p (0x10009219).
    std::vector<std::uint32_t> raw_rank_score = {4, 3, 2, 1};
    /// (4 − p) + byte[0x1003B324 + 4 − p] (0x1000940a–0x1000941c): the byte for p = 0
    /// is the first byte of the string "po_" at 0x1003B328 (0x70).
    std::vector<std::uint32_t> ore_rank_score = {116, 6, 3, 1};

    /// The original tables (docs/re/annoscene.md §5 step 3).
    static std::vector<std::uint8_t> original_ore_exclusion();
    static std::vector<std::uint32_t> original_raw_width_rank();
    static std::vector<std::uint32_t> original_ore_width_rank();
    /// Empty when valid, else one message naming the field ("LinkTables.rounds: ...").
    [[nodiscard]] std::string validate() const;
};

/// The original's number of city passes after the building scheduler (D:
/// 0x1002d9a0): the default of SimConfig::city_pass_count and the length of a
/// default Seams::city_pass.
inline constexpr std::uint32_t kOriginalCityPassCount = 8;

/// One building list: a name for messages and the bit the list contributes to
/// the lookup masks of FindByTile and of the rect queries.
struct ListSpec {
    std::string name;
    std::uint32_t lookup_mask = 0;
};

/// The group -> list map, the scheduling order and the lookup orders as data
/// (D: Create/Attach/Remove tables 0x1002604c, 0x10026034, 0x10025348,
/// 0x10025330; scheduler 0x10023a50; FindByTile 0x10023c80; rect query
/// 0x10023f50; docs/sim/state.md §3). The original numbers its lists L0..L5 in
/// object order at `index + 0x58 + 0x20·i`.
struct ListLayout {
    std::vector<ListSpec> lists;
    /// Visiting order of the building scheduler.
    std::vector<ListId> schedule;
    /// FindByTile set order.
    std::vector<ListId> lookup_order;
    /// Rect query order.
    std::vector<ListId> rect_order;
    /// Index (prodtype - first_prodtype) / block_size.
    std::vector<ListId> block_to_list;
    std::uint32_t first_prodtype = 1;
    std::uint32_t block_size = 32;
    /// The original's table default for numbers outside 1..192 (original-
    /// compatible decode only).
    ListId default_list = 4;
    /// Which list plays which part in rules that the original hard-wires to a
    /// group. Hashed with the layout and validated, so a modded layout changes
    /// drought centring, defeat and the house rules visibly instead of silently.
    struct Roles {
        /// Drought centre (docs/sim/state.md §5.8), defeat_pending on removal
        /// (§7.3): the market/Kontor group.
        ListId market_list = 5;
        /// House class (byte0e 0x82, house_counts, §4.6).
        ListId house_list = 3;
        /// Service counters.
        ListId public_list = 1;
        ListId military_list = 2;
        /// The default list of Plain-class numbers outside the table.
        ListId production_list = 4;
        /// Never scheduled in the original (market stalls).
        ListId stall_list = 0;
    } roles;

    /// The list a PRODTYPE belongs to; nullopt when it is not a building
    /// (below first_prodtype or beyond the table: roads, walls and scenery are
    /// tile objects only; D: Place skips Create for them). A zero block_size
    /// (rejected by validate) gives nullopt.
    [[nodiscard]] std::optional<ListId> list_for(std::uint32_t prodtype) const;
    /// Empty when valid, else one message naming the field
    /// ("ListLayout.schedule: ...").
    [[nodiscard]] std::string validate() const;
    /// FNV-1a 64 of the layout's field list (state_fields.hpp).
    [[nodiscard]] std::uint64_t digest() const;
    /// The original's layout (docs/sim/state.md §3).
    static ListLayout original();
};

/// Table sizes, divisors and periods. Every default is the original's value
/// (D unless a line says otherwise); docs/sim/state.md §3. Hashed in the
/// header scope; the members are listed in the order of the stream.
struct SimConfig {
    // ---- table sizes (D unless noted)
    /// World table at +0x6530; islands.size() <= island_slots.
    std::uint32_t island_slots = 256;
    /// Island table at +0x38 = the city ring length.
    std::uint32_t city_slots = 16;
    /// The owner value "nobody" (D 0x1007c790, the 4-bit nibble in the
    /// original); never a usable city number. The owner column is u32 (§4.8),
    /// so any value fits.
    std::uint32_t neutral_owner = 15;
    /// Player table +4..+0x84.
    std::uint32_t player_slots = 32;
    /// Skipped by the bankruptcy tick (D 0x1001b900).
    std::uint32_t neutral_player = 31;
    /// SKTIMER (D 0x1009cf30).
    std::uint32_t script_timers = 5;
    /// MONOPOL table (D 0x1009d2a0): 20 raw materials + 8.
    std::uint32_t monopoly_entries = 28;
    /// First loop s = 0..19; at most 32 (s is a shift count into a u32 mask)
    /// and at most monopoly_entries.
    std::uint32_t monopoly_raw_slots = 20;
    /// Public-need counters, index +6..+0x45 (D); at most 32
    /// (BuildingType::service_mask is a u32).
    std::uint32_t service_bits = 32;
    /// Index +0x46.. (D).
    std::uint32_t house_groups = 5;
    /// Player research table (D 0x10019190).
    std::uint32_t research_entries = 65;
    /// 0x38 (production.hpp).
    std::uint32_t warehouse_goods = kOriginalWarehouseGoods;
    /// Goods charged by Place: 0x29..0x2d (D 0x10027a77).
    std::uint32_t build_goods_first = 0x29;
    std::uint32_t build_goods_count = 5;
    /// Kontor sell/buy lists (D 0x73); commands carry the list as u32.
    std::uint32_t trade_lists = 2;
    /// Island sweep chunk edge in tiles (D 0x1008b760); also the drought grid
    /// (D 0x1008aeb0).
    std::uint32_t chunk_edge = 8;
    /// The list constructor creates one null element (D 0x100236a0, A7).
    std::uint32_t list_initial_size = 1;
    /// D 0x10026a00.
    std::uint32_t list_min_growth = 10;
    /// The figure registry's smallest growth: its free-slot search 0x10099130 grows a
    /// full vector to n + n/4 but at least 100 (D 0x10099185..0x100991a1; the explicit
    /// slot path 0x1009734b..0x1009736d has the same form), so a fresh registry grows
    /// 1 -> 100 -> 125 (docs/re/annowelt.md "Unit registry"). Valid as list_min_growth.
    std::uint32_t figure_min_growth = 100;
    /// Passes after the building scheduler (D 0x1002d9a0).
    std::uint32_t city_pass_count = kOriginalCityPassCount;
    // ---- scheduler divisors and periods (D)
    /// count = d·n/500 + 1
    std::uint32_t building_div = 500;
    std::uint32_t city_ring_div = 50;
    std::uint32_t player_div = 50;
    std::uint32_t figure_a_div = 250;
    std::uint32_t figure_b_div = 50;
    std::uint32_t objmgr_div = 5000;
    std::uint32_t grow_period = 2000;
    std::uint32_t fauna_period = 15000;
    /// 0x1d4c
    std::uint32_t figure_expiry_ticks = 7500;
    std::uint32_t event300_gate_seconds = 300;
    std::uint32_t event300_modulus = 20;
    std::uint32_t event750_ms = 450000;
    std::uint32_t event750_modulus = 10;
    std::uint32_t monopoly_ms = 30000;
    // ---- player table (D 0x1001b840, 0x1001b900)
    std::uint32_t upkeep_period = 2000;
    std::uint32_t upkeep_slices = 15;
    std::uint32_t stat_decay_num = 104;
    std::uint32_t stat_decay_den = 105;
    std::int64_t bankrupt_money = -3000;
    std::uint32_t bankrupt_grace_ticks = 150000;
    // ---- drought (D 0x10087280, 0x1008aeb0; docs/sim/state.md §5.8)
    /// Target ring = radius / chunk_edge = 6.
    std::uint32_t drought_radius = 0x32;
    std::uint32_t drought_plateau_period = 1000;
    std::uint32_t drought_plateau_count = 120;
    std::uint32_t drought_strip_period = 50;
    /// Strips per phase: the step advances by chunk_edge up to
    /// drought_steps · chunk_edge (D 0x1008b4ed..0x1008b4fd: `add 8`, `cmp 0x40`).
    std::uint32_t drought_steps = 8;
    /// Phases per ring, the sides top, right, bottom, left (D 0x1008b50d..0x1008b51a, jump
    /// table 0x1008b5e0).
    std::uint32_t drought_phases = 4;
    // ---- monopoly climate masks (D 0x1009d2a0): index = climate 0..6
    std::vector<std::uint32_t> monopoly_climate_mask = {0x18000, 0xf009, 0xf949, 0xf06b,
                                                        0xf6b9,  0xf04d, 0xf04d};
    // ---- session (host-side agreement, not original values)
    /// Speed 400 % refused, turn based (D 0x100071b0).
    bool multiplayer = false;
    /// Whose SetSpeed counts.
    PlayerId host_player = PlayerId{0};
    /// D 0x100c8d48: the kind table is indexed by draw % kind_by_draw.size()
    /// (= draw & 7 for the original 8 entries).
    SmallFarmAnimals small_farm_animals;
    ListLayout layout = ListLayout::original();
    /// The island-link selection of a scenario load (docs/re/annoscene.md §5): read
    /// before the world exists, hashed with the configuration so a world records the
    /// tables its links were drawn with.
    LinkTables links;

    /// docs/sim/state.md §3.1. Empty when valid, else one message naming the
    /// first offending field ("SimConfig.building_div: must be non-zero").
    [[nodiscard]] std::string validate() const;
    /// FNV-1a 64 of the config's field list (state_fields.hpp).
    [[nodiscard]] std::uint64_t digest() const;
};

/// Opt-in changes. Every default is the original's behaviour; every field
/// exists only for a decision-needed row of docs/sim/state.md §13 (and two
/// cheat/QoL switches). Hashed in the header scope.
struct Rules {
    /// D1, OQ-036.
    SlotPolicy slot_policy = SlotPolicy::Original;
    /// D2, OQ-037.
    SpatialOrder spatial_order = SpatialOrder::YXSlot;
    /// D3, OQ-038: re-read begin() after every dispatch.
    bool events_drain_new = false;
    /// F24: per-call cap of pop_due (>= 1); a limit, not a behaviour switch.
    std::uint64_t events_max_dispatch = kEventsMaxDispatchDefault;
    /// D4, OQ-039: a second event pass after the figure phase.
    bool events_flush_after_figures = false;
    /// D5, OQ-040: acc += dt; < 30000 return; acc -= 30000.
    bool monopoly_every_30s = false;
    /// D6, OQ-041.
    bool player_table_full_tick = false;
    /// D7, OQ-042: the 300 s event's centre only (the draw per island per gate
    /// is kept); scripted droughts are O-51. A reserved value: what a random centre
    /// is has not been decided (OQ-042), so Rules::validate rejects true until the
    /// owner decides and it is implemented.
    bool drought_random_centre = false;
    /// D8, OQ-043.
    bool build_atomic = false;
    /// D9, OQ-044.
    bool keep_empty_city = false;
    /// D10, OQ-045: acc -= 450000 instead of acc = 0.
    bool event750_keep_overshoot = false;
    /// D11, OQ-046; PerIsland is a reserved value.
    PrngMode prng_mode = PrngMode::Global;
    /// D12, OQ-047: 0 = a spawned figure never returns.
    /// It is the default figure body: with a Seams::figure_tick installed the seam
    /// decides when a figure ends and this rule does nothing (docs/sim/state.md §5.9).
    std::uint32_t interim_figure_return_ticks = 0;
    /// F13: honour a free-build flag (never from the wire).
    bool free_build_cheat = false;
    /// Refuse placing a type whose research is not done (O-30).
    bool enforce_research = false;
    /// D21, OQ-057: the statistics decay stat · num / den in 64 bits instead of the
    /// original's u32 product, which wraps from stat >= 41 297 763 (a statistic of
    /// 50 000 000 then falls to 8 619 359). The quotient is capped at 0xFFFFFFFF.
    bool stat_decay_wide = false;
    /// D22, OQ-057: the bankruptcy deadline t + grace is compared as a signed 32-bit
    /// difference (t − deadline > 0) and never takes the value 0 ("not running"; a
    /// sum of exactly 2^32 becomes 1), so the grace period survives the u32 wrap of
    /// the tick. The original compares unsigned: within the last grace_ticks
    /// before 2^32 a player is declared bankrupt at once or the deadline lands on 0.
    bool bankrupt_deadline_wrap_safe = false;
    /// F5, OQ-133 (owner decision 2026-10-23: "Stay on the original bug with optional
    /// fix"): script timer 0 stops at 0 when a step overshoots it. The original
    /// (D 0x1009cd51..0x1009cd59) subtracts in u32 and stops the timer only on exactly
    /// 0, so an overshoot wraps the counter and the timer runs on (timers_tick).
    bool timer0_clamp = false;
    /// OQ-150 (owner check pending): the order of the island templates a scenario's
    /// links are resolved from. Sorted is the order of the original's network-game list (D
    /// AnnoScene 0x10008187–0x100082ad) and the same on every machine; Cached (the
    /// single-player InselCache.txt order) is reserved.
    IslandCandidateOrder island_candidate_order = IslandCandidateOrder::Sorted;

    /// docs/sim/state.md §3.1. Rejects every reserved value (prng_mode ==
    /// PerIsland: no per-island stream state exists, so it could not be hashed
    /// or saved; spatial_order == Original0x10027000: O-7; drought_random_centre
    /// == true: OQ-042; island_candidate_order == Cached: OQ-150), any enum byte
    /// outside its range (a value cast from a save or a config file may be
    /// anything) and events_max_dispatch == 0. Empty when valid.
    [[nodiscard]] std::string validate() const;
    /// FNV-1a 64 of the rules' field list (state_fields.hpp).
    [[nodiscard]] std::uint64_t digest() const;
};

} // namespace anno::sim
