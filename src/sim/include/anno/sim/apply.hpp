// The application of commands to a world (spec: docs/sim/state.md §8.2, §8.4). The
// public door is World::apply; this header declares the one internal entry the
// scheduler also uses for the commands of the AI seam, which are applied inside
// advance. Not part of the public API.
#pragma once

#include <anno/sim/command.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/road_drag.hpp>

namespace anno::sim {

class World;
struct TickContext;
struct CountingDraw;

/// The advance a command is applied inside (the AI seam's commands, §5.10): its
/// context and its counting draw source, so that a seam the command runs (Place,
/// Remove, S10) draws and reports through the advance like every other seam.
struct InAdvance {
    const TickContext* ctx = nullptr;
    CountingDraw* draws = nullptr;
};

/// The friend of World that applies commands (apply.cpp).
struct Commands {
    /// Validates `command` against the world's hashed state and applies it. Pure
    /// validation: a rejected command changes no hashed state. A rejection queues a
    /// CommandRejected UI event (host queue; inside an advance also the advance's
    /// SimOutput::events), which is not state. Total; O(size of the command).
    /// `in_advance` is null between two advances (World::apply).
    static ApplyResult apply(World& w, const Command& command, const InAdvance* in_advance);
    /// World::check_build_record: one record through Build's record check without earlier
    /// records. Read-only.
    static Reject check_build(const World& w, CityRef city, const PlacementWire& record);
    /// World::check_build_path: read-only prefix evaluation of a multi-record path against
    /// cumulative virtual placements and budget (ROADS-3).
    static RoadPathCheck check_build_path(const World& w, CityRef city,
                                          std::span<const PlacementWire> records);
    /// World::shape_road_path: the records a road drag places after the reshaper (ROADS-2b).
    static std::vector<PlacementWire> shape_road_path(const World& w, CityRef city,
                                                      std::span<const PlacementWire> records);
};

} // namespace anno::sim
