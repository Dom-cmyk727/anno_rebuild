// The hierarchical state hash made visible (spec: docs/sim/state.md §9.4).
// Built only when a caller asks for it: the hot path of World::state_hash()
// allocates nothing per scope. Names are views into string constants of the
// field list (state_fields.hpp), never owned copies.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace anno::sim {

/// One scope of the hash: its name, its ordinal when the scope repeats (island
/// 2, building slot 3), its 64-bit digest and the child scopes recorded.
struct HashNode {
    std::string_view name;
    std::uint32_t index = 0;
    bool indexed = false;
    std::uint64_t hash = 0;
    std::vector<HashNode> children;
};

/// The scopes of one state hash down to the requested depth. `root.hash` is
/// the world digest, the same value the flat `state_hash()` returns.
struct HashTree {
    HashNode root;
};

} // namespace anno::sim
