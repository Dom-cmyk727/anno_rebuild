// The island phase of World::advance (spec: docs/sim/state.md §5.4–§5.8, stages S7 and
// S13; evidence: AnnoWelt.dll island tick 0x1008b760, growth visit 0x1008b5f0, city ring
// 0x100969d0, city tick 0x1002d9a0, building scheduler 0x10023a50, object manager
// 0x10095d40, 300 s event 0x10093980, drought wrapper 0x10093a10, drought constructor
// 0x10087280, drought tick 0x1008aeb0).
//
// Every function takes the read-only TickContext, the island (and city) it works
// on and the Effects buffer for the consequences that leave the island; none of
// them touches anything else, so the island phase can be ticked in any order
// that merges the buffers in island order (docs/sim/state.md §4.11, §11).
//
// Since stage S13 the growth sweep visits its chunks (0x1008b5f0) and the drought
// machine runs (0x1008aeb0); the fauna sweep, the object bodies, the eight city passes
// and the drought tile effects are the seams of seams.hpp and do nothing by default.
// The island message manager ([island+0x30], 0x100793b0) is not modelled (O-52).
#pragma once

#include <cstdint>

#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/tick.hpp>

namespace anno::sim {

/// One island's tick (D: 0x1008b760, argument `t`, its low 16 bits used), in this
/// order: the growth sweep (grow_chunk at the cursor, then the cursor step) and the
/// fauna sweep (chunk counts min(chunks · d /
/// period + 1, chunks) with d the 16-bit difference to the sweep stamp; zero chunks
/// give 0 visits and everything after still runs, A1), the object manager, the city
/// ring, the 300 s event, then the sweep stamp becomes the 16-bit tick.
void island_tick(const TickContext& ctx, Island& island, Effects& fx);

/// The growth visit of one chunk (D: 0x1008b5f0), the chunk whose top-left tile is
/// (x, y) in island tiles: every in-bounds tile of the chunk, row-major (the edge
/// chunks of an island whose width or height is not a multiple of chunk_edge are
/// clipped, A14, F20), whose object is a growing type (TypeRegistry::find_growing)
/// goes through grow() at the tick's t, and a changed object or phase is written back
/// through the dirtying setters. Draws nothing (O-13 closed).
void grow_chunk(const TickContext& ctx, Island& island, std::uint32_t x, std::uint32_t y);

/// The sweep cursor step (A3): x += chunk_edge; at the end of a row x = 0 and y +=
/// chunk_edge; at the end of the island y = 0. `width` and `height` are the island's
/// tile dimensions.
void advance_sweep_cursor(std::uint32_t& x, std::uint32_t& y, std::uint32_t width,
                          std::uint32_t height, std::uint32_t chunk_edge);

/// The island object manager (stamp formula D 0x10095d40: min(d · n / objmgr_div + 1,
/// n) visits; traversal and cursor are the stand-in of A10, the objects being
/// Island::deposits in index order): per visit Seams::object_manager runs (default
/// none), then the cursor moves one object on, wrapping; the stamp becomes the
/// 16-bit tick. With no deposits nothing is visited (the original's do-while is
/// open, O-12).
void object_manager_tick(const TickContext& ctx, Island& island, Effects& fx);

/// The city ring (D: 0x100969d0): min(d · city_slots / city_ring_div + 1, city_slots)
/// slots from the cursor, each present city gets city_tick, the cursor moves one
/// slot per visit (wrapping), the stamp becomes the 16-bit tick.
void city_ring_tick(const TickContext& ctx, Island& island, Effects& fx);

/// One city's tick (D: 0x1002d9a0): the building scheduler first, then the city
/// passes of Seams::city_pass in their original order (default none, O-3), then the
/// city's stamp becomes the 16-bit tick.
void city_tick(const TickContext& ctx, Island& island, City& city, Effects& fx);

/// The building scheduler (D: 0x10023a50): the lists of ListLayout::schedule in that
/// order, each with min(d · n / building_div + 1, n) visits from its cursor (n
/// counts the null holes), every non-null slot reached gets tick_building; the ONE
/// shared stamp of the index becomes the 16-bit tick after the last list, so every
/// list sees the same d. The lists outside the schedule (the stalls) are never
/// visited.
void building_scheduler(const TickContext& ctx, Island& island, City& city, Effects& fx);

/// The 300 s event (D: 0x10093980): with s = t / 500 game seconds, nothing happens
/// (the stamp stays) until the 16-bit difference to the island's stamp reaches
/// event300_gate_seconds; then, unless world property 6 has bit 0x04 (no draw at
/// all), EXACTLY ONE draw is made (DrawSite::Event300, before every other test),
/// and a drought starts when draw % event300_modulus == 0, the climate is not 0 and
/// one of the city slots 0 .. min(neutral_owner, city_slots) - 1 holds a city (A5);
/// whatever happened, the stamp becomes the 16-bit s. Rules::drought_random_centre
/// (D7) is a reserved value that Rules::validate rejects: the spec does not define
/// the alternative centre (OQ-042).
void event_300s(const TickContext& ctx, Island& island, Effects& fx);

/// D: 0x10093a10, the wrapper whose only caller is the 300 s event. Does nothing if
/// the island has a drought; otherwise the first non-null building (slot order,
/// holes skipped) of the market list (ListLayout::Roles::market_list) of the first
/// city (slot order over all city_slots) that has one gives the centre, floored to
/// the chunk edge, and start_drought_at runs with SimConfig::drought_radius.
/// Returns whether a drought was started.
bool start_drought(const TickContext& ctx, Island& island);

/// The drought-area constructor (D: 0x10087280, island virtual slot +0x15c): a
/// drought at world tile (x - x % chunk_edge, y - y % chunk_edge) (the constructor
/// floors, 0x100872b8..0x100872e5) in the expanding state, ring 0, target_ring =
/// radius / chunk_edge, stamp = the 16-bit `t`. Pure: a scripted drought (O-51) will
/// call it too. It sets the record unconditionally, like the constructor (a caller that
/// must not replace a running drought, as start_drought, asks first), with a city
/// mask of drought_mask_words(city_slots) zero words (O-55).
void start_drought_at(const SimConfig& cfg, Island& island, std::uint32_t x, std::uint32_t y,
                      std::uint32_t radius, std::uint32_t t);

/// The rectangle of the drought record's current strip (D 0x1008aefb..0x1008b014, see
/// DroughtStrip): ring 0 the centre chunk [x, x + e) x [y, y + e), unclipped; ring r >= 1
/// the side `phase` of the ring at distance r chunks, clipped to the island; empty for
/// a phase of 4 or more.
DroughtStrip drought_strip(const SimConfig& cfg, const Island& island, const Drought& d);

/// The drought machine (D 0x1008aeb0, docs/sim/state.md §5.8), a no-op without a
/// record. Plateau (state 0): a count per drought_plateau_period ticks, after
/// drought_plateau_count counts the state becomes 0xff. Expanding (1) or retreating
/// (0xff): a strip per drought_strip_period ticks: the stamp moves by the period (the
/// catch-up gate: never to t, one strip per call at most), Seams::drought_tiles is
/// called with the strip (default none, O-15), an expanding strip at step 0 ORs the
/// owners of its anchors samples into the city mask, then the step advances by
/// chunk_edge, at drought_steps * chunk_edge the phase, at drought_phases the ring: an
/// expanding ring past target_ring becomes the plateau at target_ring, a retreating
/// ring below 0 removes the record. Draws nothing.
void drought_tick(const TickContext& ctx, Island& island, Effects& fx);

} // namespace anno::sim
