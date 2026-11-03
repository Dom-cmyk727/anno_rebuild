// The figure registry (spec: docs/sim/state.md §4.9, §5.9; stages S7, S12, task T22).
//
// A figure is a unit the original's buildings send out: a carrier, a worker, a
// small-farm animal. The registry is the original's unit registry (D: AnnoWelt.dll
// world +0x114, vtable 0x100ccd74, tick 0x1009a8a0; docs/re/annowelt.md "Unit
// registry"): a vector of entries with the building lists' free-slot scan (0x10099130,
// A6) and a smallest growth of 100, a 16-bit stamp and the expiry list (list +0x48,
// property 0x23: a figure is destroyed 7500 ticks after its end). A figure ends once
// (end_figure, the original's unit slot +0x110 = 0x1003b6e0 and its Remove(unit, 0)):
// the home building's counter is released at that moment and the figure waits on the
// expiry list until phase C frees its slot. The registry's other list (+0x58) and its
// phase D serve the pooled sector records, never a figure (OQ-131, OQ-134): they are
// not modelled. With no behaviour seam installed a figure never moves, and it ends
// only through the interim return rule (D12, OQ-047, off by default), so the
// building's counter it holds stays raised.
//
// Figure positions are Q24.8 fixed point (D13, OQ-048); the original's are x87
// floats with no oracle.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/resident.hpp>
#include <anno/sim/slot_list.hpp>

namespace anno::sim {

struct Effects; // tick.hpp

/// A position in 1/256 tile (Q24.8); the original is an x87 float with no oracle
/// (D13, OQ-048). The i32 holds tiles up to +-2^23, a limit of this type: the world
/// edge (kWorldEdgeBound) must stay below it, which the assertion below enforces.
/// Path to raise both: widen the members to i64, or lower the fraction bits.
struct FixedPos {
    std::int32_t x = 0, y = 0;
    friend constexpr bool operator==(const FixedPos&, const FixedPos&) = default;
};
static_assert(kWorldEdgeBound <= (1U << 23),
              "FixedPos (Q24.8 in an i32) cannot hold a tile coordinate of this world edge");

/// The step of a transport job (docs/re/transport.md §1, T24): the job's current
/// step, D the list node at job +0xc (A = 0x1005ad10 the search and the walk to the
/// source, B = 0x100551b0 the load, C = 0x10057ae0 the walk home and the unload).
enum class TransportStep : std::uint8_t {
    /// The job has not run yet: the first figure call runs step A's start.
    Start = 0,
    /// Step A: walking to the chosen building.
    ToSource = 1,
    /// Step B: the load wait.
    Load = 2,
    /// Step C: walking home.
    ToHome = 3,
    /// No step left: the figure ends (the end path 0x1003af00).
    Done = 4
};

/// What a transport job keeps between calls (the job's steps and the figure's walk,
/// docs/sim/state.md §5.9). Data, not limits: the route is as long as the search found.
struct TransportState {
    TransportStep step = TransportStep::Start;
    /// Ticks left of the current wait (step B, at the wait rate of 0x10035260).
    std::uint32_t wait = 0;
    /// The walk: waypoints in Q24.8 tiles, the first where the walk started; empty when
    /// the figure does not walk (the original's route list +0x48, built by 0x10034df0
    /// from the searcher's path; here tile centres of the stand-in search, OQ-161).
    std::vector<FixedPos> route;
    /// The waypoint walked to next (1 .. route.size(); route.size() + 1 = arrived).
    std::uint32_t next = 0;
    /// The distance walked from route[next - 1] towards route[next], in 2^-24 tile.
    std::uint32_t along = 0;
    /// Step A's chosen building (D: step +4 / +6 keep its tile; the load and the abort
    /// look the building up there).
    BuildingId target;
    /// The amount the target reserved for this figure (the cargo's +0x1c, D 0x1005aef1).
    std::uint32_t reserved_88 = 0;
};

/// The step of a harvest job (docs/sim/forestry.md 2, 3.2; D AnnoWelt job factory 0x1003b0e0,
/// jobs 0x29/0x2a -> 0x1003b255): step A (Start: search and claim a tile; ToTree: the walk), step B
/// (Cut: the wait, then the cut), step C (ToHome: the walk home and the delivery), then the end.
enum class HarvestStep : std::uint8_t { Start = 0, ToTree = 1, Cut = 2, ToHome = 3, Done = 4 };

/// The first micro-step of a figure's life in ticks: the constructor's remaining 0.001 at rate
/// 0.001 (D 0x10035ce0) is 1.0 time unit, 5 ticks at the figure tick's 0.2 scale (0x10034760). The
/// integer model of docs/sim/forestry.md 2 (E2.0): saved, consumed once across visits. A fact of
/// the original's figure constructor, not a limit.
inline constexpr std::uint32_t kHarvestStartDelay = 5;

/// The jobs whose figures run the harvest body: the job factory gives 0x29 (ERNTE) and 0x2a
/// (MAEHER) the same constructor (D 0x1003b480 byte table -> 0x1003b255).
[[nodiscard]] constexpr bool harvest_job(std::uint32_t job) {
    return job == 0x29 || job == 0x2a;
}

/// What a harvest job keeps between calls (docs/sim/forestry.md 2). Present on a figure exactly
/// when the spawn of a Farm-payload home created it (never chosen by the job id alone: 0x2a is
/// also the small farm's worker job).
struct HarvestState {
    HarvestStep step = HarvestStep::Start;
    /// Ticks of the first micro-step still owed (kHarvestStartDelay at the spawn, 0 afterwards,
    /// never re-armed; docs/sim/forestry.md 3.2).
    std::uint32_t start_delay = kHarvestStartDelay;
    /// Ticks left of the cut wait (Cut).
    std::uint32_t wait = 0;
    /// The claimed tile in world coordinates (step +4/+6); meaningful in ToTree and Cut only.
    TileXY target;
    /// The walk, as TransportState's: waypoints, the next one and the distance along the segment.
    std::vector<FixedPos> route;
    std::uint32_t next = 0;
    std::uint32_t along = 0;
    friend bool operator==(const HarvestState&, const HarvestState&) = default;
};

/// The step of a resident job (job 0x35; docs/sim/population-live.md section 3): A = Start (the
/// search from the house and the choice of a stand; ToStall the walk), B = Wait (the fixed wait,
/// then the purchase at the stand: P-8, the stall wait/attach state is simplified), C = ToHome (the
/// walk home and the delivery of every cargo slot), then Done (the figure ends).
enum class ResidentStep : std::uint8_t { Start = 0, ToStall = 1, Wait = 2, ToHome = 3, Done = 4 };

/// What a resident job keeps between calls (MAP-POP-LIVE-1 L1, stream 128; the job itself is L2).
/// Present on a figure exactly when a House spawned it (never chosen by the job id alone). The
/// walk is TransportState's: waypoints, the next one and the distance along the segment.
struct ResidentState {
    ResidentStep step = ResidentStep::Start;
    /// Ticks left of the current wait (Wait).
    std::uint32_t wait = 0;
    std::vector<FixedPos> route;
    std::uint32_t next = 0;
    std::uint32_t along = 0;
    /// The chosen stand or public building (invalid until step A chose one).
    BuildingId target;
    /// The walker's cargo slots, SimConfig::resident_cargo_slots of them (the original's 3,
    /// D 0x10057d70): goods 0 = empty; a slot holding goods with amount 0 is not empty.
    std::vector<ResidentCargo> cargo;
    friend bool operator==(const ResidentState&, const ResidentState&) = default;
};

/// Raw units per ton in a ship hold: amounts are exact multiples of 1/256 ton (the original's 8.8
/// cargo amounts, docs/re/ship-founding.md section 10.7).
inline constexpr std::uint64_t kHoldUnitsPerTon = 256;

/// Whole tons to raw hold units (x256), exact; nullopt when the product does not fit 64 bits.
/// The one conversion at a whole-ton boundary (the version-2 map manifests are in whole tons).
[[nodiscard]] constexpr std::optional<std::uint64_t> hold_units_from_tons(std::uint64_t tons) {
    if (tons > UINT64_MAX / kHoldUnitsPerTon) {
        return std::nullopt;
    }
    return tons * kHoldUnitsPerTon;
}

/// One hold of a ship: a goods identity and an exact amount in raw units of 1/256 ton (MAP-2,
/// docs/sim/ship-start-integration.md §2; stream 117). u64, not the original's u16: every whole-ton
/// u32 amount the earlier representation could hold (x256 < 2^40) still fits, and the original's
/// capacity limit is not inherited. Holds are an ordered list; two holds of one goods stay two
/// entries (the ship's four holds of the baseline are wood 50, food 10, tools 50, tools 30 t, i.e.
/// 12800, 2560, 12800 and 7680 raw units). A sum of holds is computed with a checked add.
struct ShipHold {
    GoodsId goods = 0;
    std::uint64_t amount_88 = 0;
    friend constexpr auto operator<=>(const ShipHold&, const ShipHold&) = default;
};

/// The kind of a ship's current or pending motion segment (docs/sim/ship-navigation.md 12.4, 12.7).
/// Idle: no order is being sailed (the idle or stop segment); Turn: a planner turn node in progress
/// (the original's kind 3, state bit 0 clear); TurnPhase: the stepper's own 1/16 turn phase (state
/// bit 0 set, the facing changed by one at its start); Move: a movement segment (a chunk of at most
/// 1.0 length along a lattice direction).
enum class NavSegKind : std::uint8_t { Idle = 0, Turn = 1, TurnPhase = 2, Move = 3 };

/// A pending node of the plan (the original's list node): a movement of `length_q32` along `dir8`
/// at `speed_q32`, or a planner turn node that sets `facing` and lasts turn_step at the turn speed.
struct NavNode {
    NavSegKind kind = NavSegKind::Move; // Move or Turn only
    std::uint8_t dir8 = 0;              // Move: 0 W, 1 SW, 2 S, 3 SE, 4 E, 5 NE, 6 N, 7 NW
    std::uint8_t facing = 0;            // Turn: the dir16 facing it sets when it starts
    std::uint64_t length_q32 = 0;       // 2^-32 tile (a diagonal step is 6074000896)
    std::uint32_t speed_q32 = 0;        // base speed before the factor, 2^-32 tile per stepper unit
    friend constexpr bool operator==(const NavNode&, const NavNode&) = default;
};

/// The current segment (the original's +0x20..+0x34, +0x38): all integers.
struct NavSegment {
    NavSegKind kind = NavSegKind::Idle;
    std::uint8_t dir8 = 0;              // Move only
    std::uint32_t factor_percent = 100; // V applied to this segment, 1 .. 100
    std::uint32_t speed_q32 = 0;        // base speed (before the factor)
    /// Length still to sail in this segment, in q32/500 (= q32 * 500): the integrator's budget
    /// scale, so a partial step leaves no sub-unit fraction behind
    /// (docs/sim/ship-navigation.md 12.4).
    std::uint64_t remaining_x500 = 0;
    friend constexpr bool operator==(const NavSegment&, const NavSegment&) = default;
};

/// An accepted move order (docs/sim/ship-navigation.md 12.1). `routed` is the original's phase 1.
struct NavOrder {
    std::uint64_t order_id = 0; // a world counter, never reused
    bool routed = false;
    std::int32_t target_x_q8 = 0, target_y_q8 = 0;
    friend constexpr bool operator==(const NavOrder&, const NavOrder&) = default;
};

/// The persisted motion state of a navigating ship (docs/sim/ship-navigation.md 12.5). Present
/// exactly when the figure's type has navigation facts. `Figure::pos` is derived from `pos_u`
/// (validated).
struct NavMotion {
    std::optional<NavOrder> order;
    /// The fair admission ticket (12.2); present only with an order.
    std::optional<std::uint64_t> ticket;
    NavSegment seg;
    std::vector<NavNode> nodes; // pending nodes, next first
    /// The ship's own stepper stamp (the original's +0x42), distinct from Figure::stamp.
    std::uint16_t last = 0;
    /// Position per axis (x, y) in tile * 2^32 * 500, and the diagonal residue (< 2^24).
    std::int64_t pos_u[2] = {0, 0};
    std::uint32_t res24[2] = {0, 0};
    /// MAP-NAV-3 N4 (stream 123): the current hit points in the unit of NavFacts::max_hp (the
    /// original's signed 16-bit +0x6c, docs/re/ship-navigation.md 7.5); read only by the speed
    /// factor at a movement segment start. nullopt = explicitly UNKNOWN (an import that does not
    /// know it): the speed then uses the full-health term (an adaptation, docs/sim/
    /// ship-navigation.md 12.14). 0 is a legal value; at most max_hp.
    std::optional<std::uint32_t> health;
    friend bool operator==(const NavMotion& a, const NavMotion& b) {
        return a.order == b.order && a.ticket == b.ticket && a.seg == b.seg && a.nodes == b.nodes &&
               a.last == b.last && a.pos_u[0] == b.pos_u[0] && a.pos_u[1] == b.pos_u[1] &&
               a.res24[0] == b.res24[0] && a.res24[1] == b.res24[1] && a.health == b.health;
    }
};

/// One Q24.8 unit in pos_u: 2^-8 tile * 2^32 * 500.
inline constexpr std::int64_t kNavPosUnitsPerQ8 = std::int64_t{500} << 24;

struct Figure {
    FigureId id;
    /// FigurenHeader id / world figure kind (small farm animals: base 106 + i).
    std::uint32_t kind = 0;
    PlayerId owner;
    /// The building whose counter this figure holds; none once the figure has ended or
    /// released it (the unit's +0x78, cleared at 0x1003b6e0; the original's home
    /// destructors clear it too, docs/re/annowelt.md).
    BuildingId home;
    /// Which counter of the home building (Workshop carriers_out, Farm workers_out,
    /// SmallFarm figures_out[i]).
    std::uint32_t home_counter = 0;
    /// The original's job code: 0x2e small-farm animal, 0x0d fur trapper, 0 unknown
    /// (O-8).
    std::uint32_t job = 0;
    FixedPos pos;
    GoodsId cargo_goods = 0;
    std::uint32_t cargo_88 = 0;
    std::uint32_t hp = 0;
    /// The low 32 bits of the world tick at the spawn (the interim return rule D12
    /// reads it).
    std::uint32_t born = 0;
    /// The u16 tick the figure last acted at (the unit's +8 while it acts, D
    /// 0x1003488e; set at the spawn): a figure call works through u16(t - stamp)
    /// ticks (D 0x1003476a). The end freezes the original's +8 into expiry_stamp.
    std::uint16_t stamp = 0;
    /// The transport job (job 0x34; docs/sim/state.md §5.9). Default for a figure of
    /// another job.
    TransportState transport;
    /// Present = on the 7500-tick expiry list: the u16 tick of the figure's end
    /// (property 0x23, the unit's +8, which follows the tick while the unit acts and
    /// freezes at its Remove, D 0x1003488e / 0x10034844; docs/re/annowelt.md "Unit
    /// registry"). Not the spawn tick: S12 named it created_stamp. The default spawn
    /// does not set it (O-8).
    std::optional<std::uint16_t> expiry_stamp;
    /// MAP-2 (stream 116). The facing as one of 16 directions, 0..15 (the table of
    /// docs/re/ship-navigation.md §4; display and a later mover read it, nothing here turns
    /// a figure). 0 for every figure that no loader gave a facing: the neutral default.
    std::uint8_t dir16 = 0;
    /// The figure this one is carried by or belongs to (an embarked scout's ship, a donkey's
    /// scout); invalid = none, the default. Not `home` (a building counter). A figure and
    /// its parent have the same owner; the links never form a cycle (validate_figures).
    FigureId parent;
    /// The crew slot this figure fills in its parent (the original CREW/ENTRY index);
    /// present only with a parent, unique among that parent's children.
    std::optional<std::uint32_t> crew_slot;
    /// The ship's holds, in order. Empty for every figure that is not a ship. Not
    /// `cargo_goods` / `cargo_88`, which keep their one-load meaning (transport).
    std::vector<ShipHold> holds;
    /// MAP-2 (stream 120): D the figure's state word (slot +0x8c(0); a ship: u16 +0x40), widened.
    /// Bit 0x02 rejects the selected ship, 0x0a excludes a figure from the nearby query, 0x200e
    /// makes it unusable (figure_veto.hpp). 0 is the state of a scenario ship for every bit that
    /// matters (docs/re/ship-founding.md section 14.1).
    std::uint32_t state_word = 0;
    /// MAP-NAV-2 (stream 122): the motion state of a navigating ship.
    std::optional<NavMotion> nav;
    /// MAP-ECON-1 E2 (stream 126): the harvest job of a farm worker (docs/sim/forestry.md 2).
    std::optional<HarvestState> harvest;
    /// MAP-POP-LIVE-1 L1 (stream 128): the resident job of a house walker (docs/sim/
    /// population-live.md section 3).
    std::optional<ResidentState> resident;
};

/// D: the unit registry (world +0x114).
struct FigureRegistry {
    /// D: a vector of 16-byte entries; the slot policy is the building list's (A6).
    /// (Spelled out because SlotList's constructor is explicit: an aggregate member
    /// without an initializer is copy-list-initialised from `{}`.)
    SlotList<Figure> list = SlotList<Figure>();
    /// D: +0x20, set after phase B of the registry tick (0x1009aad9). The phase B
    /// cursor is the list's own `cursor` (D: a u16 at +0x22, widened).
    std::uint16_t stamp = 0;
    /// D: list +0x48, insertion order. (The list +0x58 and phase D hold the pooled
    /// sector records of registry slot +0x38, never a figure: not modelled, OQ-134.)
    std::vector<FigureId> expiry;
    /// DERIVED, never saved or hashed: the slot of every figure by id
    /// (rebuild_figure_index; World::check_invariants compares it with a
    /// recomputation).
    std::map<FigureId, Slot> by_id;
    /// DERIVED, never saved or hashed (MAP-NAV-3 N1): the children of every figure that has any,
    /// ascending by child id. Lets a carrier reach its whole subtree without scanning the registry.
    std::map<FigureId, std::vector<FigureId>> children;
    /// DERIVED, never saved or hashed (N1): the pending planning tickets of all ships, ascending.
    /// A ticket is unique (ids.next_ticket), so the ticket alone identifies its ship's place in
    /// the fair-admission order (docs/sim/ship-navigation.md 12.2).
    std::set<std::uint64_t> tickets;
    /// DERIVED, never saved or hashed (N1): the sailing ships (a navigating figure without a
    /// parent) by 16 x 16-tile registry cell, key {cell z, cell x} (the order of the original's
    /// scan: z rows outer, x columns inner), the ids of a cell ascending.
    std::map<std::pair<std::int32_t, std::int32_t>, std::set<FigureId>> ship_cells;
};

/// The 16 x 16-tile cell index of a Q24.8 coordinate (negative = cell 0, the original's clip).
[[nodiscard]] std::int32_t figure_cell_of(std::int32_t q8);

/// Keeps the derived indexes in step with a mutation made through `Figure&` (the registry cannot
/// see in-place edits). `note_ticket` after a ticket was assigned, consumed or cleared;
/// `note_ship_moved` after a sailing ship's position changed (pass the position before).
void note_ticket(FigureRegistry& registry, std::optional<std::uint64_t> before,
                 std::optional<std::uint64_t> after);
void note_ship_moved(FigureRegistry& registry, const Figure& ship, FixedPos before);

/// The first difference between the derived indexes and a fresh reconstruction from the list,
/// or empty.
[[nodiscard]] std::string figure_index_problem(const FigureRegistry& registry);

/// An empty registry: one list of SimConfig::list_initial_size null elements (A6,
/// the group list's constructor makes one, A7), stamp 0.
[[nodiscard]] FigureRegistry make_figure_registry(const SimConfig& cfg);

/// Stores `f` in a free slot by the group list's policy (Rules::slot_policy; docs/sim/
/// state.md §5.2) with the registry's own smallest growth SimConfig::figure_min_growth
/// (100, D 0x10099185..0x100991a1; not list_min_growth), records it in the id map and returns
/// the slot; kNoSlot, changing nothing, when the list cannot grow (a limit of the
/// slot type, SlotList::insert) or `f.id` is invalid or already in the registry.
Slot add_figure(FigureRegistry& registry, Figure f, const SimConfig& cfg, const Rules& rules);

/// The figure with this id, or nullptr (a figure that is gone is never an error).
[[nodiscard]] const Figure* find_figure(const FigureRegistry& registry, FigureId id);
[[nodiscard]] Figure* find_figure(FigureRegistry& registry, FigureId id);

/// Rebuilds the id map from the list (after a read of the state).
void rebuild_figure_index(FigureRegistry& registry);

/// Removes the figure `id` from the registry: the slot is emptied (the list keeps its
/// length, hint and cursor, as SlotList::remove), the id map forgets it, and the id
/// leaves the expiry list. Returns the removed figure, or nullopt when the registry
/// holds no such figure (the id is still taken off the list). Nothing is released: D
/// 0x10097520 (phase C) nulls the entry of the figure's slot index (the unit's slot
/// +0x40), calls the unit's destructor (slot +8 = 0x10031db0, which frees only the unit)
/// and lowers the live count +0x24 (derived, not modelled); the home counter went back
/// at the figure's end (end_figure, OQ-131).
std::optional<Figure> remove_figure(FigureRegistry& registry, FigureId id);

/// Ends `figure` on its own fields (the half of end_figure a behaviour seam may do, C2):
/// D 0x1003b6e0 (unit slot +0x110): if the figure still has a home, `fx` gets the
/// ReleaseCounter of its home counter (home slot +0x2c, which nulls the building's
/// pointer to the unit) and `home` becomes none (the unit's +0x78 = 0), so a second
/// release through the figure does nothing; then, as the Remove(unit, 0) at
/// 0x10034844, the stamp freezes at u16(t) (property 0x23). A figure that has ended
/// already (it has a stamp) is left alone. Returns true when it ended the figure now.
/// The original's last walk between the release and the Remove (0x1003af00: 0 or 15.0
/// at rate 0.01) belongs to a behaviour body (O-8); here both happen at `t`.
/// MAP-ECON-1-CLOSE-1 (docs/sim/forestry.md 3.5, "W ends by any path"): before the release
/// a harvest job that is not Done is finished while the home id still names its island:
/// the step becomes Done with no walk, wait or first delay owed (target and cargo stay
/// as history); in ToTree/Cut `fx` gets a TileEdit::Release of the target, which the
/// merge applies only under Rules::forestry_claim_hygiene and only to the worker's own
/// entry (the faithful rules write nothing beyond the traced cut/abort). A Done job, the
/// body's own end, is left exactly as it is.
bool end_figure(Figure& figure, std::uint32_t t, Effects& fx);

/// end_figure on a figure of `registry`, which also appends it to the expiry list
/// (list +0x48; phase C frees the slot at the first advance with u16(t' - stamp) >=
/// figure_expiry_ticks). The interim return rule D12 calls it in phase B.
void end_figure(FigureRegistry& registry, Figure& figure, std::uint32_t t, Effects& fx);

/// After a behaviour seam ran on `figure` (which may write the figure's own fields,
/// docs/sim/state.md §4.11 C2), the registry follows: a figure that gained an
/// expiry_stamp is appended to the expiry list and one that lost it leaves it; a
/// figure that has a stamp but still a home (a seam that set the stamp instead of
/// calling SerialWorld::end_figure) finishes its harvest job and releases its counter
/// into `fx` as end_figure does, so the counter goes back once (own design: the seam
/// protocol, which keeps the facts of validate_figures). `had_stamp` is the flag before
/// the call.
void sync_figure_lists(FigureRegistry& registry, Figure& figure, bool had_stamp, Effects& fx);

/// The structural facts of a figure registry (docs/sim/state.md §9.6; MAP-2 added the
/// dir16, parent and crew-slot facts, validated in one linear pass plus ordered lookups,
/// no ancestor chain is walked twice): the list has
/// at least one element with its hint in -1 .. n and its cursor below n, every
/// figure has a valid id below `ids.next_figure` that no other figure has, the
/// expiry list names figures of the registry once each, a figure is on the expiry
/// list exactly when it has an expiry_stamp, a figure with a stamp has no home (it
/// released its counter at its end), and the id map equals a recomputation. (That no
/// figure refers to a counter it does not hold is World::check_invariants', which
/// sees the buildings.) Empty when they hold, else one message ("figures.list: ...").
[[nodiscard]] std::string validate_figures(const FigureRegistry& registry, const IdCounters& ids);

} // namespace anno::sim
