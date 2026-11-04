// Transport: the market cart (job 0x34) and the workshop carrier (job 0x33) as the
// default body of the serial figure phase (spec: docs/sim/state.md §5.9 "Transport",
// task T24; evidence: docs/re/transport.md).
//
// Both jobs run the original's three job steps (D AnnoWelt.dll, job factory
// 0x1003b0e0): A, search the home's area for a source and walk there (the cart
// 0x1005ad10, the carrier 0x1005a530); B, wait and load (0x100551b0 / 0x100551f0);
// C, walk home and hand the cargo to the home (0x10057ae0 / 0x10057bd0); then the
// figure ends (the end path 0x1003af00). The choice of the source, its scores, the
// reservations, the loads, the area and the tile search are the original's decoded
// rules; the finer sub-cell route/retry is not decoded. The walk is integer-only
// (Q24.8 positions, the speed in 2^-24 tile per tick; OQ-162).
//
// The body reads the world through the SerialWorld and writes only its own figure
// and its Effects (store_deltas for the reservations, the loads and a workshop's
// input, goods_moves for a market's city stock, the end through
// SerialWorld::end_figure), merged right after the call (C2). It draws nothing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include <anno/sim/building.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

struct WalkPoint; // walk_search.hpp (which includes walk_cells.hpp, which needs SearchArea)

/// The job code of the market cart (D 0x1006b6d0, docs/re/transport.md §1.5).
inline constexpr std::uint32_t kJobMarketCart = 0x34;
/// The job code of the workshop carrier (D 0x100680b0, docs/re/production.md §4.1).
inline constexpr std::uint32_t kJobWorkshopCarrier = 0x33;

/// Whether the transport body runs figures of this job: the market cart and the
/// workshop carrier.
[[nodiscard]] bool transport_runs_job(std::uint32_t job);

/// The half-widths of the searcher's disc, one per row distance 0 .. r (D AnnoWelt
/// 0x10002a60: integer midpoint-circle steps, a radius of at most 3 widened by one on
/// its outer rows; the table holds bytes in the original, r is a byte).
[[nodiscard]] std::vector<std::uint32_t> disc_half_widths(std::uint32_t r);

/// The area of a building's figures (docs/re/transport.md §1.1, §3): the centre
/// column(s) cx0 .. cx1 and row(s) cy0 .. cy1 of the footprint (two when the footprint
/// is even), the type's radius and the disc's half-widths.
struct SearchArea {
    std::int32_t cx0 = 0, cx1 = 0, cy0 = 0, cy1 = 0;
    std::uint32_t radius = 0;
    std::vector<std::uint32_t> half;

    /// The candidate square (D 0x1005ad29-0x1005adbe): cx0 - r .. cx1 + r by
    /// cy0 - r .. cy1 + r, in world tiles.
    [[nodiscard]] TileRect square() const;
    /// Whether the world tile (x, y) lies in the disc (the block fill 0x10003780 blocks
    /// the cells outside; the row distance is taken from the centre rows, the column
    /// distance from the centre columns: likely, the fill's arithmetic is not read in
    /// full).
    [[nodiscard]] bool contains(std::int32_t x, std::int32_t y) const;
};

/// The area of `b` (footprint at its rotation, radius = the type's action radius,
/// HEAD[0x11] = type +0x65, AnnoGame 0x10007d90).
[[nodiscard]] SearchArea search_area(const Building& b, const BuildingType& type);

/// What a building has of goods `g` for a figure: the original's slot +0xa4 (docs/re/
/// transport.md §1.4): a producer's output store minus its reservation (the line of a
/// small farm or a weapons workshop chosen by the goods as the original's), a market's
/// city stock of `g` (`city` is the market's city; nullptr reads 0); 0 for a building
/// without either. Signed, as the original's arithmetic.
[[nodiscard]] std::int64_t available(const Building& b, const BuildingType& type, GoodsId g,
                                     const City* city = nullptr);

/// The output line slots +0xa4 / +0xb4 / +0xa8 address for goods `g` (0 or 1).
[[nodiscard]] std::uint32_t store_line(const Building& b, const BuildingType& type, GoodsId g);

/// A workshop's input priority for goods `g` (D slot +0xd8 = 0x10068500): the input's
/// stock plus the reserved and carried amounts of the workshop's carriers that carry
/// `g`, in steps of the input's need: below 30 -> 0x80, below 60 -> 0x60, else -1 (and
/// -1 for goods that are no input).
[[nodiscard]] std::int32_t workshop_priority(const SerialWorld& world, const Building& workshop,
                                             const BuildingType& type, GoodsId g);

/// What a source offers a workshop (D slot +0xd4 = 0x100728f0): the best of
/// p + avail · 192 / cap over the workshop's inputs that the source holds (its output 0
/// is the goods or ALL = 0x8e, or its output 1 is the goods), with avail >= 1 t and
/// p = workshop_priority > 0; -1 when none. `goods` takes the input chosen.
[[nodiscard]] std::int64_t workshop_offer(const SerialWorld& world, const TypeRegistry& types,
                                          const Building& workshop, const BuildingType& type,
                                          const Building& source, const City* source_city,
                                          GoodsId& goods);

/// Step A's choice: the source building, its goods and the decoded tile path to it.
struct SourceChoice {
    BuildingId building;
    GoodsId goods = 0;
    std::vector<TileXY> path;
};

/// MAP-ECON-1 E3/E4: a diagnostic tally of the harvest workers and their farms (measurement and
/// test ledger only; not sim state, never hashed or saved, never drawn). Filled through the
/// TransportStats sink when a host or test installs one (World::set_transport_stats).
struct ForestryStats {
    std::uint64_t searches = 0;         ///< step A starts with a home
    std::uint64_t searches_failed = 0;  ///< no candidate or no route: straight to step C
    std::uint64_t cuts = 0;             ///< successful planned cuts (each credited capacity_88...)
    std::uint64_t cuts_failed = 0;      ///< a cut whose predicate failed (no credit)
    std::uint64_t raw_credited_88 = 0;  ///< cargo credited by successful cuts
    std::uint64_t raw_delivered_88 = 0; ///< cargo handed to a home's raw store (StoreDelta::Input)
    std::uint64_t raw_lost_88 = 0;      ///< cargo of workers that ended without a delivery
    std::uint64_t aborts = 0;           ///< home gone at an arrival
    std::uint64_t drops = 0;            ///< enhanced revalidation dropped a target
    std::uint64_t cut_mismatches = 0;   ///< merge-time re-check of a planned cut failed (must be 0)
    /// Per farm building: raw consumed and output made by its production steps (the tick's own
    /// arithmetic, docs/sim/forestry.md 7), for the E4 ledger.
    struct FarmLedger {
        std::uint64_t consumed_88 = 0, made_88 = 0;
    };
    std::map<BuildingId, FarmLedger> farms;
};

/// MAP-POP-LIVE-1 L2: a diagnostic tally of the resident (house walker) jobs; measurement and test
/// ledger only, never sim state, never hashed or saved. Filled through the TransportStats sink.
struct ResidentStats {
    std::uint64_t searches = 0;           ///< step A starts with a home
    std::uint64_t searches_failed = 0;    ///< no candidate or no route: straight to step C
    std::uint64_t arrivals = 0;           ///< step B arrivals
    std::uint64_t arrivals_empty = 0;     ///< an arrival with no building of the stand lists
    std::uint64_t purchases = 0;          ///< cargo slots filled
    std::uint64_t bought_88 = 0;          ///< what the suppliers returned
    std::uint64_t faults = 0;             ///< a typed ResidentFault in a choice, bill or add (P-3)
    std::uint64_t deliveries = 0;         ///< cargo slots handed to a home (job side)
    std::uint64_t delivered_88 = 0;       ///< their amounts
    std::uint64_t deliveries_refused = 0; ///< merge side: a stock overflow, nothing changed
    std::uint64_t lost_88 = 0;            ///< cargo of residents that ended without a delivery
    std::uint64_t no_path_home = 0;       ///< step C found no path home (the job ends, cargo kept)
    std::uint64_t home_gone = 0;          ///< the home was gone or lost the walker's slot
    /// The per-goods ledger of the whole live chain (MAP-POP-LIVE-1 L5; same rules, diagnostic).
    /// Job side, at the purchase: the cargo amounts the suppliers returned, the city store they
    /// took (a stall takes it, a public building only bills), and the billed deltas (the REQUESTED
    /// amounts).
    std::map<GoodsId, std::uint64_t> bought_by_goods, taken_by_goods, billed_by_goods;
    /// Job side, at the home arrival: what was handed to the house.
    std::map<GoodsId, std::uint64_t> delivered_by_goods;
    /// Merge side, what actually changed: the city store taken by resident purchases, the billed
    /// counters raised, the deliveries applied (amount), the house stock raised at the delivered
    /// goods' own slot and by the IdSec replays (stock units, 1/16 of an amount unit).
    std::map<GoodsId, std::uint64_t> merged_taken_by_goods, merged_billed_by_goods;
    std::map<GoodsId, std::uint64_t> merged_delivered_by_goods;
    /// Deliveries the merge did not apply (the house or its walker slot was gone), and the cargo
    /// of walkers that ended without handing it over, per goods.
    std::map<GoodsId, std::uint64_t> merge_dropped_by_goods, lost_by_goods;
    std::map<GoodsId, std::uint64_t> stock_primary_by_goods, stock_replay_by_goods;
    /// 16 x amount x (the length of the IdSec chain the delivery walked): what the stock must have
    /// risen by in total (a chain may add twice to one slot when a substitute shares it).
    std::map<GoodsId, std::uint64_t> stock_expected_by_goods;
};

/// MAP-POP-LIVE-1 L5: the operation counters of one house (scale and proportionality checks,
/// docs/sim/population-live.md section 20). Diagnostic only: never state, hashed or saved.
struct HouseWork {
    std::uint64_t visits = 0;              ///< house_tick calls (taken or skipped)
    std::uint64_t kernel_calls = 0;        ///< house_kernel calls (one per visit)
    std::uint64_t kernel_taken = 0;        ///< kernel results that evaluated (gate taken)
    std::uint64_t spawn_draws = 0;         ///< HouseWalkerTarget draws
    std::uint64_t spawn_requests = 0;      ///< spawn requests pushed
    std::uint64_t searches = 0;            ///< resident step A starts (choose_stand calls)
    std::uint64_t rect_queries = 0;        ///< candidate rectangle queries
    std::uint64_t rect_hits = 0;           ///< ids the queries returned
    std::uint64_t candidates_scored = 0;   ///< house_choice evaluations of candidates
    std::uint64_t candidates_accepted = 0; ///< candidates with score >= the purchase gate
    std::uint64_t walk_searches = 0;       ///< walk searches run
    std::uint64_t walk_expansions = 0;     ///< cells popped by those searches
    std::uint64_t upgrade_scores = 0;      ///< upgrade_scores evaluations
    std::uint64_t upgrade_terminal = 0;    ///< C3: end-of-ladder houses at their cap (named outcome)
    std::uint64_t upgrade_special = 0;     ///< C3: declared special successors at their cap
};

/// One growth step that changed a house's residents (the kernel's result, committed).
struct HouseGrowthRecord {
    BuildingId building;
    std::uint32_t tick = 0;
    int mood = 0;
    std::int32_t residents_before = 0, residents_after = 0;
    std::int32_t vermehr = 0; ///< the group's VermehrTab entry the mood selected
    /// MAP-POP-CLOSE-1 F2: what the House really holds after the tick (read from the committed
    /// state, 0 when the house was removed), the group's cap (Wohnmax * 64) and whether the tick
    /// removed the house. `residents_after` stays the kernel's number.
    std::int32_t residents_committed = 0;
    std::int32_t cap_64 = 0;
    bool removed = false;
};

/// One progression transition of a house, with every number the contract names.
struct HouseTransitionRecord {
    enum class Kind : std::uint8_t { Upgrade, UpgradeRefused, Downgrade, Remove };
    Kind kind = Kind::Upgrade;
    BuildingId building;
    std::uint32_t tick = 0;
    TypeId from, to;
    std::uint32_t old_group = 0, new_group = 0;
    std::int32_t residents_before = 0, residents_after = 0;
    std::uint32_t houses_old_before = 0, houses_old_after = 0;
    std::uint32_t houses_new_before = 0, houses_new_after = 0;
    std::int64_t counter_old_before = 0, counter_old_after = 0;
    std::int64_t counter_new_before = 0, counter_new_after = 0;
    std::vector<std::int64_t> spent_by_good; ///< city build goods spent, per build good
    std::vector<std::int64_t> stock_by_good; ///< city store change of the build goods
    bool variant_drawn = false;
    std::uint16_t variant_draw = 0;
    std::uint32_t variant_index = 0;
};

/// One stage bit set by the stage pass.
struct StageUnlockRecord {
    std::uint32_t tick = 0;
    CityRef city;
    std::uint32_t stage = 0, group = 0, count = 0;
    std::int64_t residents_at_or_above = 0;
    std::uint32_t city_mask_before = 0, city_mask_after = 0;
    std::uint32_t player_flags_before = 0, player_flags_after = 0;
};

/// The population path's tally: per-house work, the stage pass and the records above.
struct PopulationStats {
    std::map<BuildingId, HouseWork> houses;
    std::uint64_t stage_passes = 0;      ///< stage_pass calls that took the gate
    std::uint64_t stage_evaluations = 0; ///< stage records examined (bit not yet set)
    std::vector<HouseGrowthRecord> growth;
    std::vector<HouseTransitionRecord> transitions;
    std::vector<StageUnlockRecord> unlocks;
    /// The records above are bounded: a long run keeps the first kRecordCap of each and counts
    /// the rest here.
    static constexpr std::size_t kRecordCap = 1U << 20;
    std::uint64_t records_dropped = 0;
    [[nodiscard]] HouseWork total() const {
        HouseWork t;
        for (const auto& [id, w] : houses) {
            t.visits += w.visits;
            t.kernel_calls += w.kernel_calls;
            t.kernel_taken += w.kernel_taken;
            t.spawn_draws += w.spawn_draws;
            t.spawn_requests += w.spawn_requests;
            t.searches += w.searches;
            t.rect_queries += w.rect_queries;
            t.rect_hits += w.rect_hits;
            t.candidates_scored += w.candidates_scored;
            t.candidates_accepted += w.candidates_accepted;
            t.walk_searches += w.walk_searches;
            t.walk_expansions += w.walk_expansions;
            t.upgrade_scores += w.upgrade_scores;
            t.upgrade_terminal += w.upgrade_terminal;
            t.upgrade_special += w.upgrade_special;
        }
        return t;
    }
};

/// A diagnostic tally of why a search ended, filled when `choose_source` is passed a
/// non-null pointer (task T27, measurement only; not sim state, not hashed, not drawn).
/// Exactly one outcome counter is bumped per call that reaches the candidate stage.
struct TransportStats {
    std::uint64_t no_candidate = 0; ///< (i) no building passed the filter
    std::uint64_t unreached =
        0; ///< (ii-a) candidates, search completed but path empty (goal reached, no route built)
    std::uint64_t no_goal_reached =
        0;                       ///< (ii-b) search completed but no goal cell was visited at all
    std::uint64_t low_score = 0; ///< (iii) a goal was reached but no score beat the start
    std::uint64_t found = 0;     ///< (iv) a source was chosen
    /// (v) exact cells unavailable (the retained tile-only placement lost metadata).
    std::uint64_t incomplete = 0;
    // Historical T29 stand-in diagnostics. The decoded search no longer fills
    // frontier/edge/corner/outside_disc; home/goal_no_road remain layout observations.
    /// Per blocked tile inside the search grid that is an 8-neighbour of a reached tile
    /// (unreached searches only): the type id of the tile's object (0 = no object), each
    /// tile counted once per search. T30 reads the top ids with `anno-dump anlagen`.
    std::map<std::uint32_t, std::uint64_t> frontier;
    /// Unreached searches whose reached set touches the search grid's border (the area
    /// cut: a road leaves the home's area). Counted once per search.
    std::uint64_t edge = 0;
    /// Senders whose footprint has no road tile among its 4-neighbours; counted once per
    /// call that reaches the candidate stage.
    std::uint64_t home_no_road = 0;
    /// Candidates (unique per search) whose footprint has no road tile among its
    /// 4-neighbours; counted per candidate.
    std::uint64_t goal_no_road = 0;
    /// Diagonal steps from a reached tile to a passable, unreached tile that the corner
    /// rule refused ("no diagonal passes a blocked orthogonal neighbour", unreached
    /// searches only); counted per step occurrence (one (tile, direction) pair = one).
    std::uint64_t corner = 0;
    /// Unreached searches in which every candidate's footprint lies outside the area's
    /// disc (candidates come from `area.square()` but only tiles inside the disc are
    /// walkable, T27 review). Counted once per search.
    std::uint64_t outside_disc = 0;
    /// Starts whose position lies outside the search rectangle and are silently skipped
    /// (T33.4 F1 diagnostic). Counted once per skipped start.
    std::uint64_t start_outside_rect = 0;
    /// MAP-ECON-1: the harvest workers' tally (same sink, same rules: diagnostic only).
    ForestryStats forestry;
    /// MAP-POP-LIVE-1 L2: the residents' tally (same sink, same rules).
    ResidentStats residents;
    /// MAP-POP-LIVE-1 L5: the House path's operation counters and records (same rules).
    PopulationStats population;
    /// MAP-ECON-1 E4 ledger (diagnostic only): what the carts and carriers took at their load, per
    /// source building; what they offered at their unload into a market's city (before the city's
    /// capacity clip), and the cargo of those that ended without an unload, per goods.
    std::map<BuildingId, std::uint64_t> loaded_from;
    std::map<GoodsId, std::uint64_t> unload_offered;
    std::map<GoodsId, std::uint64_t> cart_lost;
};

/// Step A's choice for a cart of market `home` (D 0x1005ad10 with the callback
/// 0x1005a900) or a carrier of workshop `home` (D 0x1005a530 with the callback
/// 0x1005a400): nullopt when no candidate passes the filter or none is reached with a
/// score above the start (0 for the cart, -1 for the carrier). `capacity_88` is the
/// figure type's capacity (the carrier's early stop at 3/4 of it), `layer` its decoded
/// walking-table layer.
/// `stats`, when non-null, is bumped in exactly one bucket by a call that reaches the
/// candidate filter (TransportStats); a call whose island or city is gone bumps none.
[[nodiscard]] std::optional<SourceChoice>
choose_source(const SerialWorld& world, const TypeRegistry& types, std::uint32_t job,
              const Building& home, const BuildingType& home_type, CityRef city,
              std::uint32_t capacity_88, std::uint32_t layer, TransportStats* stats = nullptr);

/// The decoded tile path from the Q24.8 position `from` to the first reached entrance
/// of `home` inside its area (step C's walk, D 0x10057ae0 / callback 0x10054240).
[[nodiscard]] std::optional<std::vector<TileXY>>
path_home(const Island& island, const TypeRegistry& types, const Building& home,
          const BuildingType& home_type, FixedPos from, std::uint32_t layer);

// ---- shared walk helpers (the transport body's own, exported for the harvest body, MAP-ECON-1 E3)

/// The tile of a Q24.8 coordinate (floor division by 256).
[[nodiscard]] std::int32_t tile_of_q8(std::int32_t q);
/// The searcher's start/goal point of a Q24.8 position: its tile and the sub-cell
/// floor(3 * fraction) per axis.
[[nodiscard]] WalkPoint walk_point_of(FixedPos p);
/// The walk points of a building's entrance points (rotated, docs/re/transport.md 1.6).
[[nodiscard]] std::vector<WalkPoint> entrance_walk_points(const Building& b,
                                                          const BuildingType& type);
/// The centre of a tile in Q24.8.
[[nodiscard]] FixedPos tile_centre(TileXY t);
/// Whether the world tile (x, y) is inside the building's footprint at its rotation.
[[nodiscard]] bool tile_in_footprint(const Building& b, const BuildingType& type, std::int32_t x,
                                     std::int32_t y);
/// A walk's state: the waypoints, the next one and the distance along the segment
/// (TransportState's and HarvestState's fields).
struct WalkRef {
    std::vector<FixedPos>& route;
    std::uint32_t& next;
    std::uint32_t& along;
};
/// Starts a walk from `pos` along the decoded tile path: the figure's spot, then the centres of the
/// path's tiles after the first (the path starts on the figure's own tile).
void start_walk(FixedPos pos, const std::vector<TileXY>& path, WalkRef w);
/// Walks `ticks` at `speed` (2^-24 tile per tick), moving `pos`; the ticks left after the arrival
/// (the route is then cleared), nullopt while the walk goes on. A figure without speed never
/// arrives.
[[nodiscard]] std::optional<std::uint64_t> walk_ticks(FixedPos& pos, WalkRef w, std::uint64_t ticks,
                                                      std::uint32_t speed);

/// The length of a straight walk segment in 2^-24 tile: |d| on an axis, |d| · √2 on a
/// diagonal (the segment builder 0x1002fde0 multiplies by 1.41421354f: 23726566 per
/// tile), the integer square root otherwise (the first segment from a figure's spot).
[[nodiscard]] std::uint64_t segment_length_q24(FixedPos a, FixedPos b);

/// One call of the transport body on `fig` (a figure of a job transport_runs_job()
/// accepts): it works through u16(t - fig.stamp) ticks of the job (D the figure tick
/// 0x10034760: an action ends, the next step starts, a step that starts no action
/// is done in the same call), then sets the stamp to u16(t). Returns false, changing
/// nothing, for a figure whose type the registry does not know (no FigurenHeader.dat
/// data: no speed, capacity or layer) and for a carrier whose home is not a HANDWERK
/// workshop (the weapons workshop's carrier is stage 2); those keep the default
/// behaviour.
bool transport_tick(const TickContext& ctx, SerialWorld& world, Figure& fig);

} // namespace anno::sim
