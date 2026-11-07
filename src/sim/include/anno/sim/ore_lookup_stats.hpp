// Operation counters of the mine source lookup (MAP-EXTRACT-1 X5). Diagnostic only: never state,
// hashed or saved.
#pragma once

#include <cstdint>

namespace anno::sim {

/// Deterministic operation counters of the lookup and the bind (diagnostic: never state, hashed or
/// saved). A lookup visits every ore object of ITS island once (a linear scan of
/// Island::ore_objects, no spatial index), tests the cheap reach geometry first, and only for an
/// ore in reach the figure type and the island's deposit table. It never touches the island's
/// buildings, other islands or cities, so its cost does not depend on how many unrelated mines or
/// cities exist.
struct OreLookupStats {
    std::uint64_t lookups = 0;
    std::uint64_t ore_objects_examined =
        0;                               ///< ore objects visited (= ore_objects.size() per lookup)
    std::uint64_t in_reach = 0;          ///< of them inside the reach table
    std::uint64_t ware_matches = 0;      ///< of those: an ore symbol of the mine's ware
    std::uint64_t deposits_examined = 0; ///< deposit-table entries compared (lookup and bind)
};

} // namespace anno::sim
