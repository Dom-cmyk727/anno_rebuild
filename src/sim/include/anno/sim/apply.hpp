// The application of commands to a world (spec: docs/sim/state.md §8.2, §8.4). The
// public door is World::apply; this header declares the one internal entry the
// scheduler also uses for the commands of the AI seam, which are applied inside
// advance. Not part of the public API.
#pragma once

#include <anno/sim/command.hpp>
#include <anno/sim/reject.hpp>

namespace anno::sim {

class World;

/// The friend of World that applies commands (apply.cpp).
struct Commands {
    /// Validates `command` against the world's hashed state and applies it. Pure
    /// validation: a rejected command changes no hashed state. A rejection queues a
    /// CommandRejected UI event (host queue; with `inside_advance` also the advance's
    /// SimOutput::events), which is not state. Total; O(size of the command).
    static ApplyResult apply(World& w, const Command& command, bool inside_advance);
};

} // namespace anno::sim
