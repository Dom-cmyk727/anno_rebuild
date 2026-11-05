// Pure search, state.md section5.9 clauses3-8. No world/state mutation.
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <anno/sim/walk_cells.hpp>
namespace anno::sim {
/// Already quantized point: world tile and floor(3*within-tile fraction).
/// Three subcells per axis are the encoded mask format, not a map-size limit.
struct WalkPoint {
    TileXY tile;
    std::uint32_t col = 1, row = 1;
    friend bool operator==(const WalkPoint&, const WalkPoint&) = default;
};
struct SearchVisit {
    WalkPoint point;
    std::uint16_t score = 0, high = 0;
    friend bool operator==(const SearchVisit&, const SearchVisit&) = default;
};
enum class GoalAction { Expand, Skip, Stop };
struct SearchDecision {
    GoalAction action = GoalAction::Expand;
    /// Replace the selected result, like engine slot+0x90(node,4).
    bool select = false;
};
using SearchCallback = std::function<SearchDecision(const SearchVisit&)>;
struct SearchResult {
    std::optional<SearchVisit> selected;
    std::vector<TileXY> route; // start through selected goal; length is size()-1
    std::uint16_t cost = 0;
    std::uint64_t enqueued = 0, popped = 0;
    bool stopped = false, complete = true;
    /// Starts skipped because their position lies outside the search rectangle
    /// (T33.4 F1 diagnostic).
    std::uint64_t start_outside_rect = 0;
    std::string error;
    friend bool operator==(const SearchResult&, const SearchResult&) = default;
};
/// D AnnoWelt0x10003aa0: seed even a blocked subcell, flood free orthogonal
/// neighbours, return reached-edge keys in bits12..15. Invalid subcells return0.
[[nodiscard]] std::uint16_t start_keys(std::uint16_t mask9, std::uint32_t col, std::uint32_t row);
/// Copies input scratch; source island/types/tables must remain unchanged.
/// Callback is required and must be deterministic for identical visits. A Skip does not
/// skip later subcells; Stop returns immediately (AnnoWelt0x1000373f..0x1000377d).
/// Bad tables/layer give error, unsupported retained placements give complete=false.
[[nodiscard]] SearchResult search(const WalkCells&, const WalkTables&, std::uint32_t layer,
                                  std::span<const WalkPoint> starts,
                                  std::span<const WalkPoint> goals, const SearchCallback&);
/// MAP-POP-CLOSE-1 C3 (P-15): whether `route` (tiles, the first is the start's tile) is a route
/// the search's own FORWARD expansion would offer from `start`: every step is one cardinal tile
/// step (no teleport), is among the moves the arrival state offers (the same `steps` order, with
/// the start flood for the first cell and the came-from code afterwards), enters a destination
/// whose edge keys admit the mover (`low & geometry`), and the layer's admission table admits the
/// destination (no forbidden terrain). It replays the forward rule; it does not search. A route
/// found by a search from the OTHER end is legal forward only if this answers true. `bad_step` is
/// the index of the first refused step (1-based) or 0.
struct RouteCheck {
    bool legal = false;
    std::uint64_t bad_step = 0;
    std::string reason;
};
/// `leave_exempt` / `enter_exempt` (the resident endpoint adapter): the first / last step is only
/// required to be a cardinal neighbour step inside the rectangle, not to satisfy the arrival-state,
/// edge-key and admission rules. A figure leaves a building it stands in (a stall) and enters its
/// home through the entrance tile; those tiles are closed to THROUGH traffic in the layer tables,
/// so the forward rule cannot offer the step although the walker starts or ends there.
[[nodiscard]] RouteCheck route_legal(const WalkCells&, const WalkTables&, std::uint32_t layer,
                                     const WalkPoint& start, std::span<const TileXY> route,
                                     bool leave_exempt = false, bool enter_exempt = false);
} // namespace anno::sim
