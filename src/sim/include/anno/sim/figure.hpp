// The figure registry, creation only (spec: docs/sim/state.md §4.9, §5.9, stage S7).
//
// A figure is a unit the original's buildings send out: a carrier, a worker, a
// small-farm animal. The registry is the original's unit registry (D: AnnoWelt.dll
// world +0x114, vtable 0x100ccd74, tick 0x1009a8a0): a vector of entries with the
// group list's slot allocation (A6, assumed: the allocator 0x10097250 is unread), a
// 16-bit stamp, the expiry list (property 0x23, entries die after 7500 ticks) and the
// deferred-delete list. Stage S7 has the registry state and the creation path the
// effect merge needs; stage S12 adds the removal and the list helpers the registry
// tick uses (its phases B/C/D run in World::advance, scheduler.cpp). With no behaviour
// seam installed a figure never moves, and it ends only through the expiry list (which
// the default spawn never fills, O-8) or the interim return rule (D12, OQ-047, off by
// default), so the building's counter it holds stays raised.
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

/// A position in 1/256 tile (Q24.8); the original is an x87 float with no oracle
/// (D13, OQ-048). The i32 holds tiles up to +-2^23, a limit of this type: the world
/// edge (kWorldEdgeBound) must stay below it, which the assertion below enforces.
/// Path to raise both: widen the members to i64, or lower the fraction bits.
struct FixedPos {
    std::int32_t x = 0, y = 0;
};
static_assert(kWorldEdgeBound <= (1U << 23),
              "FixedPos (Q24.8 in an i32) cannot hold a tile coordinate of this world edge");

struct Figure {
    FigureId id;
    /// FigurenHeader id / world figure kind (small farm animals: base 106 + i).
    std::uint32_t kind = 0;
    PlayerId owner;
    /// The building whose counter this figure holds.
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
    /// Present = on the 7500-tick expiry list: the u16 tick of the figure's end
    /// (property 0x23, the unit's +8, which follows the tick while the unit acts and
    /// freezes at its Remove, D 0x1003488e / 0x10034844; docs/re/annowelt.md "Unit
    /// registry"). Not the spawn tick: S12 named it created_stamp. The default spawn
    /// does not set it (O-8).
    std::optional<std::uint16_t> expiry_stamp;
    /// On the deferred-delete list (phase D).
    bool marked_for_delete = false;
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
    /// D: list +0x48, insertion order.
    std::vector<FigureId> expiry;
    /// D: list +0x58.
    std::vector<FigureId> deferred;
    /// DERIVED, never saved or hashed: the slot of every figure by id
    /// (rebuild_figure_index; World::check_invariants compares it with a
    /// recomputation).
    std::map<FigureId, Slot> by_id;
};

/// An empty registry: one list of SimConfig::list_initial_size null elements (A6,
/// the group list's constructor makes one, A7), stamp 0.
[[nodiscard]] FigureRegistry make_figure_registry(const SimConfig& cfg);

/// Stores `f` in a free slot by the group list's policy (SimConfig::list_min_growth,
/// Rules::slot_policy; docs/sim/state.md §5.2), records it in the id map and returns
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
/// leaves the expiry and the deferred list. Returns the removed figure, or nullopt when
/// the registry holds no such figure (the id is still taken off both lists). The caller
/// releases the home building's counter (the world's merge rule).
///
/// Evidence and stand-in (OQ-131): phase C of the original destroys through 0x10097520,
/// which nulls the entry of the figure's slot (read through the unit's slot +0x40),
/// calls the unit's slot +8 and lowers the live count word +0x24 (derived, not
/// modelled). Phase D goes another way: registry slot +0x3c = 0x10097770 unlinks the
/// unit's record from its list and the phase-A sector cells, returns it to the record
/// pool and calls the unit's slot +8, and does not touch the entry vector. Whether the
/// unit destructor (unread) frees the registry slot, unlinks the unit from the expiry
/// and deferred lists and releases the home counter is open; the model does all three
/// for both phases, so no list keeps a stale entry and no counter stays raised.
std::optional<Figure> remove_figure(FigureRegistry& registry, FigureId id);

/// Puts `figure` on the deferred-delete list and marks it, unless it is marked already
/// (the interim return rule D12 and a behaviour seam use it). D: registry slot +0x40 =
/// 0x10097720 with a zero flag appends to the list +0x58 without asking whether the unit
/// is on it already (a non-zero flag deletes at once through 0x10097770); the "once"
/// is own design, so a figure is never released twice (validate_figures, OQ-131).
void mark_figure_for_delete(FigureRegistry& registry, Figure& figure);

/// After a behaviour seam ran on `figure` (which may write the figure's own fields,
/// docs/sim/state.md §4.11 C2), the registry's lists follow its flags: a figure that
/// gained an expiry_stamp is appended to the expiry list and one that lost it leaves
/// it; one that became marked_for_delete is appended to the deferred list and one that
/// was unmarked leaves it (own design: the seam protocol of stage S12, so a seam cannot
/// break the list invariants of validate_figures). `had_stamp` and `was_marked` are the
/// flags before the call.
void sync_figure_lists(FigureRegistry& registry, const Figure& figure, bool had_stamp,
                       bool was_marked);

/// The structural facts of a figure registry (docs/sim/state.md §9.6): the list has
/// at least one element with its hint in -1 .. n and its cursor below n, every
/// figure has a valid id below `ids.next_figure` that no other figure has, the
/// expiry and deferred lists name figures of the registry once each, a figure is on
/// the expiry list exactly when it has an expiry_stamp and on the deferred list
/// exactly when it is marked_for_delete, and the id map equals a recomputation.
/// Empty when they hold, else one message ("figures.list: ...").
[[nodiscard]] std::string validate_figures(const FigureRegistry& registry, const IdCounters& ids);

} // namespace anno::sim
