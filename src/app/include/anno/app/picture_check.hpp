// Comparison of picture chunks with an independent reference (round dx, DX1; docs/perf/README.md
// "Verifier"). The reference is a fresh scene loaded with the desired building list and rendered
// synchronously. A chunk is equal only if its ground, objects, terrain and sea field are equal to
// the reference's. A key that is missing on either side is a failure, never a pass. Aggregate
// object counts are diagnostics only: they cannot detect a count-preserving substitution.
#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <anno/render/frame.hpp>

namespace anno::app {

/// The differing layers of the compared chunks, and the keys that had no chunk on one side.
struct ChunkDiff {
    std::size_t compared = 0;
    std::size_t ground = 0, objects = 0, terrain = 0, field = 0;
    std::size_t missing = 0;
    /// Chunks with at least one object pixel in the reference (a comparison that covers objects).
    std::size_t with_objects = 0;
    [[nodiscard]] bool equal() const {
        return compared > 0 && ground == 0 && objects == 0 && terrain == 0 && field == 0 &&
               missing == 0;
    }
};

/// The chunk of `key`, or nullptr when there is none (a missing chunk).
using ChunkLookup = std::function<const render::Chunk*(const render::ChunkKey&)>;

/// Compares the chunks of `keys`: `candidate` (what the picture shows) against `reference` (a fresh
/// build).
ChunkDiff compare_chunks(const std::vector<render::ChunkKey>& keys, const ChunkLookup& candidate,
                         const ChunkLookup& reference);

} // namespace anno::app
