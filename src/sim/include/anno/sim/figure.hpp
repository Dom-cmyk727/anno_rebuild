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
#include <string>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/slot_list.hpp>

namespace anno::sim {

struct Effects; // tick.hpp

/// A position in 1/256 tile (Q24.8); the original is an x87 float with no oracle
/// (D13, OQ-048). The i32 holds tiles up to +-2^23, a limit of this type: the world
/// edge (kWorldEdgeBound) must stay below it, which the assertion below enforces.
/// Path to raise both: widen the members to i64, or lower the fraction bits.
struct FixedPos {
    std::int32_t x = 0, y = 0;
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

/// One hold of a ship: a goods identity and whole tons (MAP-2, docs/sim/ship-start-
/// integration.md §2). Whole tons, not 8.8: nothing here shifts them. Holds are an
/// ordered list; two holds of one goods stay two entries (the ship's four holds of the
/// baseline are wood 50, food 10, tools 50, tools 30).
struct ShipHold {
    GoodsId goods = 0;
    std::uint32_t tons = 0;
    friend constexpr auto operator<=>(const ShipHold&, const ShipHold&) = default;
};

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
};

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
bool end_figure(Figure& figure, std::uint32_t t, Effects& fx);

/// end_figure on a figure of `registry`, which also appends it to the expiry list
/// (list +0x48; phase C frees the slot at the first advance with u16(t' - stamp) >=
/// figure_expiry_ticks). The interim return rule D12 calls it in phase B.
void end_figure(FigureRegistry& registry, Figure& figure, std::uint32_t t, Effects& fx);

/// After a behaviour seam ran on `figure` (which may write the figure's own fields,
/// docs/sim/state.md §4.11 C2), the registry follows: a figure that gained an
/// expiry_stamp is appended to the expiry list and one that lost it leaves it; a
/// figure that has a stamp but still a home (a seam that set the stamp instead of
/// calling SerialWorld::end_figure) releases its counter into `fx` as end_figure does,
/// so the counter goes back once (own design: the seam protocol, which keeps the facts
/// of validate_figures). `had_stamp` is the flag before the call.
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
