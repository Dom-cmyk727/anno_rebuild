#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <utility>

#include <anno/sim/walk_search.hpp>
namespace anno::sim {
namespace {
struct Node {
    TileXY tile;
    std::uint16_t score = 0;
};
// D AnnoWelt0x1000d501 and0x1000474f: lowest unsigned16 score, equal scores FIFO.
// Insertion number is widened; no original pool/tree capacity is inherited.
using Open = std::map<std::pair<std::uint16_t, std::uint64_t>, Node>;
void enqueue(Open& open, SearchResult& result, Node node) {
    open.emplace(std::pair{node.score, result.enqueued}, node);
    ++result.enqueued;
}
bool inside(TileRect rect, TileXY p) {
    const auto x = std::int64_t{p.x} - rect.x0, y = std::int64_t{p.y} - rect.y0;
    return x >= 0 && y >= 0 && x < rect.w && y < rect.h;
}
} // namespace

std::uint16_t start_keys(std::uint16_t mask9, std::uint32_t col, std::uint32_t row) {
    // The 3x3 shape is fixed by the nine-bit record (0x1009206c..0x100920e2).
    // A different subcell format requires changing this flood and its edge keys.
    const std::uint32_t edge = 3;
    if (col >= edge || row >= edge) {
        return 0;
    }
    std::vector<std::uint32_t> pending{col + edge * row};
    std::uint16_t reached = static_cast<std::uint16_t>(
        1U << pending.front()); // D0x10003b98: seed10, even if formerly blocked
    std::uint16_t keys = 0;
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto x = pending[i] % edge, y = pending[i] / edge;
        if (x == 0) {
            keys = static_cast<std::uint16_t>(keys | 0x1000);
        }
        if (y == edge - 1) {
            keys = static_cast<std::uint16_t>(keys | 0x2000);
        }
        if (x == edge - 1) {
            keys = static_cast<std::uint16_t>(keys | 0x4000);
        }
        if (y == 0) {
            keys = static_cast<std::uint16_t>(keys | 0x8000);
        }
        // Mathematical cardinal offsets, not an original storage limit.
        const std::array<TileXY, 4> moves{{{0, -1}, {-1, 0}, {1, 0}, {0, 1}}};
        for (const auto d : moves) {
            const auto nx = static_cast<std::int32_t>(x) + d.x,
                       ny = static_cast<std::int32_t>(y) + d.y;
            if (nx < 0 || ny < 0 || nx >= static_cast<std::int32_t>(edge) ||
                ny >= static_cast<std::int32_t>(edge)) {
                continue;
            }
            const auto bit = static_cast<std::uint32_t>(nx) + edge * static_cast<std::uint32_t>(ny);
            if ((reached & (1U << bit)) == 0 && (mask9 & (1U << bit)) == 0) {
                reached = static_cast<std::uint16_t>(reached | (1U << bit));
                pending.push_back(bit);
            }
        }
    }
    return keys;
}
namespace {
void seed(Open& open, SearchResult& result, WalkCells& cells, std::span<const WalkPoint> starts) {
    // D0x10003d9b..0x10003f87, fn2 and0x10003f73: do not deduplicate starts.
    for (const auto& point : starts) {
        if (!inside(cells.rect(), point.tile) || point.col >= 3 || point.row >= 3) {
            continue;
        }
        auto cell = cells.get(point.tile);
        const auto raw = cells.record(point.tile);
        if (!raw.supported) {
            result.complete = false;
            continue;
        }
        cell.high = static_cast<std::uint16_t>((cell.high & 0xfc07) | 7);
        cell.low =
            static_cast<std::uint16_t>(cell.low | start_keys(raw.mask9, point.col, point.row));
        if (cells.set(point.tile, cell)) {
            enqueue(open, result, Node{point.tile, 0});
        }
    }
}
} // namespace

namespace {
struct Move {
    std::uint32_t dir = 0, key = 0, diagonal = 0;
    bool turn = false;
};
struct Steps {
    // Four cardinal axes in the encoded geometry, not a search-size cap.
    std::array<Move, 4> moves{};
    std::uint32_t count = 0;
};
Steps steps(std::uint16_t low, std::uint16_t high) {
    // D AnnoWelt0x10006a40 and specials0x100071c0/0x10007770/0x10007b00/0x10007ea0.
    Steps s;
    s.count = 3;
    switch (high & 0x3ff) {
    case 0x80: {
        const std::uint32_t g = low & 7U;
        s.moves[0] = {6, (g & 4) << 4, 7, true};
        s.moves[1] = {2, (g & 1) << 2, 1, true};
        s.moves[2] = {0, (g >> 1) & 1, 0, false};
        break;
    }
    case 0x82: {
        const std::uint32_t g = (low >> 3) & 7U;
        s.moves[0] = {0, (g >> 2) & 1, 1, true};
        s.moves[1] = {4, (g & 1) << 4, 3, true};
        s.moves[2] = {2, (g & 2) << 1, 0, false};
        break;
    }
    case 0x84: {
        const std::uint32_t g = (low >> 6) & 7U;
        s.moves[0] = {6, (g & 1) << 6, 5, true};
        s.moves[1] = {2, g & 4, 3, true};
        s.moves[2] = {4, (g & 2) << 3, 0, false};
        break;
    }
    case 0x86: {
        const std::uint32_t g = (low >> 9) & 7U;
        s.moves[0] = {0, g & 1, 7, true};
        s.moves[1] = {4, (g & 4) << 2, 5, true};
        s.moves[2] = {6, (g & 2) << 5, 0, false};
        break;
    }
    default: {
        const std::uint32_t n = low >> 12;
        s.count = 4;
        s.moves[0] = {6, (n & 8) << 3, 0, false};
        s.moves[1] = {2, (n & 2) << 1, 0, false};
        s.moves[2] = {0, n & 1, 0, false};
        s.moves[3] = {4, (n & 4) << 2, 0, false};
        break;
    }
    }
    return s;
}
std::optional<TileXY> neighbour(TileXY p, std::uint32_t dir, bool backwards = false) {
    std::int64_t x = p.x, y = p.y;
    const std::int32_t sign = backwards ? -1 : 1;
    switch (dir) {
    case 0:
        x -= sign;
        break;
    case 2:
        y += sign;
        break;
    case 4:
        x += sign;
        break;
    case 6:
        y -= sign;
        break;
    default:
        return {};
    }
    if (x < std::numeric_limits<std::int32_t>::min() ||
        x > std::numeric_limits<std::int32_t>::max() ||
        y < std::numeric_limits<std::int32_t>::min() ||
        y > std::numeric_limits<std::int32_t>::max()) {
        return {};
    }
    return TileXY{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}
std::uint16_t geometry(std::uint32_t dir) {
    // D AnnoWelt admission tests0x10006a81/0x10006c69/0x10006dd0/0x10006e8c.
    switch (dir) {
    case 6:
        return 0x2111;
    case 2:
        return 0x8444;
    case 0:
        return 0x4888;
    case 4:
        return 0x1222;
    default:
        return 0;
    }
}
void expand(Open& open, SearchResult& result, WalkCells& cells, const WalkTables& tables,
            std::uint32_t layer, Node node, WalkCell current) {
    const auto order = steps(current.low, current.high);
    for (std::size_t i = 0; i < order.count; ++i) {
        const auto m = order.moves[i];
        const auto next = neighbour(node.tile, m.dir);
        if (!next || !inside(cells.rect(), *next)) {
            continue;
        }
        auto destination = cells.get(*next);
        if ((destination.low & geometry(m.dir)) == 0 ||
            (admission(tables, layer, destination.high) & m.key) == 0) {
            continue;
        }
        // Destination code before marking; signed intermediate then modulo2^16.
        // D0x10007571..0x100075a1, no clamp or relaxation.
        const auto orth = static_cast<std::int32_t>(cost(tables, layer, destination.high, m.dir));
        const auto increment =
            m.turn ? static_cast<std::int32_t>(cost(tables, layer, destination.high, m.diagonal)) -
                         orth
                   : orth;
        const auto score =
            static_cast<std::uint16_t>(static_cast<std::int32_t>(node.score) + increment);
        destination.high = static_cast<std::uint16_t>((destination.high & 0xfc00) | (0x80 | m.dir));
        if (cells.set(*next, destination)) {
            enqueue(open, result, Node{*next, score});
        }
    }
}
} // namespace

namespace {
using GoalMasks = std::map<TileXY, std::uint16_t>;
GoalMasks mark_goals(WalkCells& cells, std::span<const WalkPoint> goals) {
    GoalMasks masks;
    for (const auto& point : goals) {
        if (!inside(cells.rect(), point.tile) || point.col >= 3 || point.row >= 3) {
            continue;
        }
        auto cell = cells.get(point.tile);
        cell.high = static_cast<std::uint16_t>(cell.high | 0x400); // D0x10004068
        if (cells.set(point.tile, cell)) {
            masks[point.tile] =
                static_cast<std::uint16_t>(masks[point.tile] | (1U << (point.col + 3 * point.row)));
        }
    }
    return masks;
}
struct GoalReply {
    bool expand = true, stop = false;
};
GoalReply visit_goals(SearchResult& result, const WalkTables& tables, const GoalMasks& goals,
                      Node node, WalkCell cell, const SearchCallback& callback) {
    GoalReply reply;
    const auto goal = goals.find(node.tile);
    if ((cell.high & 0x400) == 0 || goal == goals.end() || goal->second == 0) {
        return reply;
    }
    const auto code = cell.high & 0x3ffU;
    const std::size_t row = code >= 0x80 && code <= 0x86 && code % 2 == 0 ? (code - 0x80) / 2 : 4;
    // D AnnoWelt0x100036fb..0x1000377d: AND replies, but an explicit stop returns immediately.
    for (const auto& entry : tables.goal_order[row]) {
        if ((goal->second & entry.mask) == 0) {
            continue;
        }
        const SearchVisit visit{WalkPoint{node.tile, entry.col, entry.row}, node.score, cell.high};
        const auto decision = callback(visit);
        if (decision.select) {
            result.selected = visit;
        }
        if (decision.action == GoalAction::Stop) {
            reply.expand = false;
            reply.stop = true;
            return reply;
        }
        reply.expand = reply.expand && decision.action == GoalAction::Expand;
    }
    return reply;
}
std::string check_search(const WalkTables& tables, std::uint32_t layer,
                         const SearchCallback& callback) {
    if (auto e = validate_walk_tables(tables); !e.empty()) {
        return e;
    }
    if (layer >= tables.layers.size()) {
        return "search: layer outside tables";
    }
    if (!callback) {
        return "search: missing callback";
    }
    // Closed admission codes are the original's visited set, and are needed for
    // first-discovery termination. Do not silently substitute another rule.
    if (admission(tables, layer, 7) != 0) {
        return "search: start/disc code must be closed";
    }
    for (std::uint16_t code = 0x80; code <= 0x86; code += 2) {
        if (admission(tables, layer, code) != 0) {
            return "search: came-from codes must be closed";
        }
    }
    for (std::size_t row = 0; row < 5; ++row) {
        std::uint16_t used = 0;
        for (const auto& entry : tables.goal_order[row]) {
            if (entry.col >= 3 || entry.row >= 3 ||
                entry.mask != (1U << (entry.col + 3 * entry.row)) || (used & entry.mask) != 0) {
                return "search: invalid goal-order permutation";
            }
            used = static_cast<std::uint16_t>(used | entry.mask);
        }
        if (used != 0x1ff) {
            return "search: incomplete goal-order row";
        }
    }
    return {};
}
} // namespace

namespace {
void trace_route(SearchResult& result, WalkCells& cells) {
    if (!result.selected) {
        return;
    }
    result.cost = result.selected->score;
    auto tile = result.selected->point.tile;
    // A valid first-discovery chain cannot contain more distinct tiles than this
    // input rectangle. This data-derived bound diagnoses corrupt chains, no fixed cap.
    std::uint64_t remaining = std::uint64_t{cells.rect().w} * cells.rect().h;
    while (remaining != 0 && inside(cells.rect(), tile)) {
        --remaining;
        result.route.push_back(tile);
        const auto code = cells.get(tile).high & 0x3ffU;
        if (code == 7) {
            std::reverse(result.route.begin(), result.route.end());
            return;
        }
        if (code != 0x80 && code != 0x82 && code != 0x84 && code != 0x86) {
            break;
        }
        const auto parent = neighbour(tile, code & 7U, true);
        if (!parent) {
            break;
        }
        tile = *parent;
    }
    result.route.clear();
    result.error = "search: came-from chain has no start";
}
} // namespace
SearchResult search(const WalkCells& input, const WalkTables& tables, std::uint32_t layer,
                    std::span<const WalkPoint> starts, std::span<const WalkPoint> goals,
                    const SearchCallback& callback) {
    SearchResult result;
    result.error = check_search(tables, layer, callback);
    if (!result.error.empty()) {
        result.complete = false;
        return result;
    }
    WalkCells cells = input; // source scratch and all simulation inputs remain unchanged
    result.complete = cells.complete();
    Open open;
    seed(open, result, cells, starts);
    const auto goal_masks = mark_goals(cells, goals);
    while (!open.empty()) {
        const Node node = open.begin()->second;
        open.erase(open.begin());
        ++result.popped;
        const auto cell = cells.get(node.tile);
        const auto reply = visit_goals(result, tables, goal_masks, node, cell, callback);
        if (reply.stop) {
            result.stopped = true;
            break;
        }
        if (reply.expand) {
            expand(open, result, cells, tables, layer, node, cell);
        }
    }
    trace_route(result, cells);
    result.complete = result.complete && cells.complete();
    // deviation: finer-route validation/retry and its one-transition vetoes are
    // not implemented (state5.9(6),0x100054ef..0x100055d5), as T33.3 requires.
    return result;
}
} // namespace anno::sim
