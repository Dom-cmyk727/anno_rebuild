// The harvest job (MAP-ECON-1 E3; docs/sim/forestry.md 3.2, 3.5). Integer only; no draw.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include <anno/sim/driver.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/mason.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/walk_search.hpp>

namespace anno::sim {

namespace {

/// PRODTYPE 226 = ROHSTWACHS (AnnoGame 0x100138e0; the cut's test `cmp eax, 0xe2`, 0x1008739a).
constexpr std::uint32_t kRohstwachs = 226;

/// The per-call view of the tile fields the body writes (docs/sim/forestry.md 3.2): every read
/// consults it before the island, every write goes to it and, in order, to Effects::tile_edits.
struct OverlayTile {
    std::optional<bool> claim;
    std::optional<std::uint32_t> object;
    std::optional<std::uint8_t> phase;
    /// The enhanced owner (Rules::forestry_claim_hygiene only): set = known in this call (an
    /// inner nullopt = no owner).
    std::optional<std::optional<FigureId>> owner;
};

struct Body {
    const TickContext& ctx;
    SerialWorld& world;
    Figure& fig;
    HarvestState& h;
    const FigureType& ft;
    IslandId island_id;
    const Island* island;
    const Building* home;
    const BuildingType* home_type;
    ForestryStats* stats;
    bool hygiene;
    bool skip;
    std::map<TileXY, OverlayTile> ov;
    bool delivered = false;

    [[nodiscard]] Slot index(TileXY t) const {
        return island != nullptr ? island->tile_index(t.x, t.y) : kNoSlot;
    }
    [[nodiscard]] const OverlayTile* view(TileXY t) const {
        const auto it = ov.find(t);
        return it != ov.end() ? &it->second : nullptr;
    }
    [[nodiscard]] std::uint32_t object(TileXY t) const {
        const OverlayTile* o = view(t);
        if (o != nullptr && o->object) {
            return *o->object;
        }
        const Slot i = index(t);
        return i == kNoSlot ? 0U : island->tiles.object(i);
    }
    [[nodiscard]] std::uint8_t phase(TileXY t) const {
        const OverlayTile* o = view(t);
        if (o != nullptr && o->phase) {
            return *o->phase;
        }
        const Slot i = index(t);
        return i == kNoSlot ? std::uint8_t{0} : island->tiles.phase(i);
    }
    [[nodiscard]] bool claimed(TileXY t) const {
        const OverlayTile* o = view(t);
        if (o != nullptr && o->claim) {
            return *o->claim;
        }
        const Slot i = index(t);
        return i != kNoSlot && island->tiles.claimed(i);
    }
    [[nodiscard]] std::optional<FigureId> owner(TileXY t) const {
        const OverlayTile* o = view(t);
        if (o != nullptr && o->owner) {
            return *o->owner;
        }
        if (island == nullptr) {
            return std::nullopt;
        }
        const auto it = island->claims.find(t);
        return it != island->claims.end() ? std::optional<FigureId>(it->second) : std::nullopt;
    }
    void edit(TileEdit::Kind kind, TileXY t) {
        world.fx.tile_edits.push_back(TileEdit{kind, island_id, t, fig.id});
    }
    void claim(TileXY t) {
        OverlayTile& o = ov[t];
        o.claim = true;
        if (hygiene) {
            o.owner = std::optional<FigureId>(fig.id);
        }
        edit(TileEdit::Kind::Claim, t);
    }
    void unclaim(TileXY t) {
        OverlayTile& o = ov[t];
        o.claim = false;
        if (hygiene) {
            o.owner = std::optional<FigureId>();
        }
        edit(TileEdit::Kind::Unclaim, t);
    }
    /// The cut through the overlay (D 0x10087340); true when it changed the tile.
    bool cut(TileXY t) {
        const std::uint32_t id = object(t);
        if (!harvest_cuttable(ctx.types, id)) {
            return false;
        }
        OverlayTile& o = ov[t];
        o.object =
            id - 1; // the type's own id ([T+4]) equals the key id - 1 (docs/re/forestry.md 2.12)
        o.phase = static_cast<std::uint8_t>(phase(t) & 0x80U);
        edit(TileEdit::Kind::Cut, t);
        return true;
    }
    /// The worker's own claim may be cleared: faithful always (one bit, no owner, D 0x100545c0 /
    /// 0x10054530), enhanced only when this worker owns it (docs/sim/forestry.md 3.5).
    [[nodiscard]] bool may_clear(TileXY t) const { return !hygiene || owner(t) == fig.id; }
    /// Enhanced revalidation: the worker owns T and T is still a candidate (claim test aside).
    [[nodiscard]] bool valid(TileXY t) const {
        return owner(t) == fig.id &&
               harvest_candidate_type(ctx.types, object(t), fig.cargo_goods, skip);
    }

    void reset_walk() {
        h.route.clear();
        h.next = 0;
        h.along = 0;
    }
    /// The abort (home gone at an arrival, D 0x1005c260 step 1 -> 0x100545c0), then Done.
    void abort() {
        if (may_clear(h.target)) {
            unclaim(h.target);
        }
        h.wait = 0;
        reset_walk();
        h.step = HarvestStep::Done;
        if (stats != nullptr) {
            ++stats->aborts;
        }
    }
    /// Enhanced: forget the target (releasing only the worker's own claim), search again.
    void drop() {
        if (owner(h.target) == fig.id) {
            unclaim(h.target);
        }
        h.wait = 0;
        reset_walk();
        h.step = HarvestStep::Start;
        if (stats != nullptr) {
            ++stats->drops;
        }
    }
    /// Step C's start (D 0x10057ae0): inside the home's footprint no walk; else the walk home, or
    /// no path: the job ends (the cargo stays with the figure).
    void start_home() {
        h.wait = 0;
        reset_walk();
        if (home == nullptr || home_type == nullptr) {
            h.step = HarvestStep::Done;
            return;
        }
        h.step = HarvestStep::ToHome;
        if (tile_in_footprint(*home, *home_type, tile_of_q8(fig.pos.x), tile_of_q8(fig.pos.y))) {
            return;
        }
        const Island* hi = world.island(island_of(home->id));
        WalkPoint end;
        const auto path =
            hi != nullptr ? path_home(*hi, ctx.types, *home, *home_type, fig.pos, ft.layer, &end)
                          : std::nullopt;
        if (!path) {
            h.step = HarvestStep::Done; // figure slot +0xcc found no path (0x10057bb1)
            return;
        }
        start_walk(fig.pos, *path, WalkRef{h.route, h.next, h.along}, &end);
    }

    /// Step A's search (D 0x10058ae0): the candidate list through the overlay (0x10092320 mode 0,
    /// row-major over the square clipped to the island; a square whose u16 origin wraps finds
    /// nothing), then the walk search with the tiles' free sub-cells as goals, the first popped
    /// goal tile that is not claimed winning (callback 0x10058a40). The tile path to it, or none.
    struct Found {
        TileXY target;
        std::vector<TileXY> path;
        WalkPoint end; ///< the goal sub-cell the search selected (the walk ends there)
    };
    [[nodiscard]] std::optional<Found> search() const {
        if (island == nullptr || home == nullptr || home_type == nullptr) {
            return std::nullopt;
        }
        const SearchArea area = search_area(*home, *home_type);
        const std::int64_t sx = std::int64_t{area.cx0} - area.radius;
        const std::int64_t sy = std::int64_t{area.cy0} - area.radius;
        if (sx < 0 || sy < 0) {
            return std::nullopt; // the u16 origin wraps beyond the island (vector `area 3 3 3 3 5`)
        }
        const TileRect sq = area.square();
        const TileRect ir = island->rect();
        const std::int64_t x_lo = std::max<std::int64_t>(sq.x0, ir.x0);
        const std::int64_t y_lo = std::max<std::int64_t>(sq.y0, ir.y0);
        const std::int64_t x_hi =
            std::min<std::int64_t>(std::int64_t{sq.x0} + sq.w, std::int64_t{ir.x0} + ir.w);
        const std::int64_t y_hi =
            std::min<std::int64_t>(std::int64_t{sq.y0} + sq.h, std::int64_t{ir.y0} + ir.h);
        std::set<TileXY> candidates;
        std::vector<WalkPoint> goals;
        std::uint64_t scanned = 0;
        for (std::int64_t y = y_lo; y < y_hi; ++y) {
            for (std::int64_t x = x_lo; x < x_hi; ++x) {
                const TileXY t{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
                ++scanned;
                if (claimed(t) ||
                    !harvest_candidate_type(ctx.types, object(t), fig.cargo_goods, skip)) {
                    continue;
                }
                candidates.insert(t);
                // The goal sub-cells are the tile's FREE sub-cells (0x10003220: 0x1ff minus the
                // record's blocked mask, row-major, bit 4 the centre; docs/re/forestry.md 2.11).
                const std::uint32_t blocked = island->tiles.walk_mask(index(t)) & 0x1ffU;
                for (std::uint32_t row = 0; row < 3; ++row) {
                    for (std::uint32_t col = 0; col < 3; ++col) {
                        if ((blocked >> (row * 3 + col) & 1U) == 0) {
                            goals.push_back(WalkPoint{t, col, row});
                        }
                    }
                }
            }
        }
        if (stats != nullptr) {
            stats->search_tiles_scanned += scanned; // one per examined tile
            stats->search_candidates += candidates.size();
        }
        if (candidates.empty()) {
            return std::nullopt;
        }
        const auto& tables = ctx.types.walk_tables();
        if (!tables) {
            return std::nullopt;
        }
        WalkCells cells(*island, ctx.types, *tables, sq, 4, area);
        const std::array starts{walk_point_of(fig.pos)};
        std::optional<TileXY> chosen;
        const SearchResult found =
            anno::sim::search(cells, *tables, ft.layer, starts, goals, [&](const SearchVisit& v) {
                if (candidates.contains(v.point.tile) && !claimed(v.point.tile)) {
                    // D 0x10058a40: take the goal (best := node), go on only while limit > score.
                    chosen = v.point.tile;
                    return SearchDecision{
                        harvest_search_goes_on(harvest_search_limit(fig.job), v.score)
                            ? GoalAction::Expand
                            : GoalAction::Stop,
                        true};
                }
                return SearchDecision{};
            });
        if (stats != nullptr) {
            stats->search_popped += found.popped;
        }
        if (!found.complete || !found.selected || !chosen || found.route.empty()) {
            return std::nullopt;
        }
        return Found{*chosen, found.route, found.selected->point};
    }
};

} // namespace

std::optional<TileXY> harvest_pick_tile(std::uint16_t limit,
                                        const std::vector<HarvestGoalNode>& nodes) {
    std::optional<TileXY> tile;
    for (const HarvestGoalNode& n : nodes) {
        if (n.claimed) {
            continue;
        }
        tile = TileXY{n.x, n.y};
        if (!harvest_search_goes_on(limit, n.score)) {
            break;
        }
    }
    return tile;
}

bool harvest_cuttable(const TypeRegistry& types, std::uint32_t id) {
    if (id == 0) {
        return false;
    }
    const BuildingType* prev = types.find(TypeId{id - 1});
    return prev != nullptr && prev->prodtype == kRohstwachs;
}

bool harvest_candidate_type(const TypeRegistry& types, std::uint32_t id, GoodsId goods,
                            bool skip_uncuttable) {
    if (id == 0) {
        return false;
    }
    const BuildingType* t = types.find(TypeId{id});
    if (t == nullptr || t->output_goods.empty() || t->output_goods[0] != goods) {
        return false;
    }
    return !skip_uncuttable || harvest_cuttable(types, id);
}

bool harvest_tick(const TickContext& ctx, SerialWorld& world, Figure& fig) {
    const FigureType* ft = ctx.types.find_figure_type(fig.kind);
    if (ft == nullptr || !fig.harvest.has_value()) {
        return false;
    }
    const std::uint32_t t = ctx.t;
    if (mason_job(fig.job)) {
        return mason_tick(ctx, world, fig); // job 0x2c shares the state, not the body
    }
    HarvestState& h = *fig.harvest;
    if (fig.expiry_stamp.has_value()) {
        fig.stamp = static_cast<std::uint16_t>(t); // ended, waiting for phase C
        return true;
    }
    std::uint64_t ticks = static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - fig.stamp);
    if (ticks == 0) {
        return true; // D 0x10034760 step 3: no elapsed time, no work
    }
    fig.stamp = static_cast<std::uint16_t>(t);
    // The saved first micro-step (docs/sim/forestry.md 2, 3.2; E-21): consumed once, the surplus
    // goes on; a visit that does not finish it does nothing else.
    if (h.start_delay > 0) {
        const std::uint64_t used = std::min<std::uint64_t>(h.start_delay, ticks);
        h.start_delay = static_cast<std::uint32_t>(h.start_delay - used);
        ticks -= used;
        if (h.start_delay > 0) {
            return true;
        }
    }
    const Building* home = world.find_building(fig.home);
    const BuildingType* home_type = home != nullptr ? ctx.types.find(home->type) : nullptr;
    if (home != nullptr && !std::holds_alternative<Farm>(home->payload) &&
        !std::holds_alternative<SmallFarm>(home->payload)) {
        home = nullptr; // validated away (World::harvest_problem); never a stand-in home
        home_type = nullptr;
    }
    const IslandId island_id = island_of(fig.home);
    Body b{ctx,
           world,
           fig,
           h,
           *ft,
           island_id,
           world.island(island_id),
           home,
           home_type,
           ctx.transport_stats != nullptr ? &ctx.transport_stats->forestry : nullptr,
           ctx.rules.forestry_claim_hygiene,
           ctx.rules.forestry_skip_uncuttable,
           {}};
    const std::uint32_t speed = ft->speed_q24;
    // Enhanced revalidation at each visit, before the worker walks or waits on (§3.5).
    if (b.hygiene && (h.step == HarvestStep::ToTree || h.step == HarvestStep::Cut) &&
        !b.valid(h.target)) {
        b.drop();
    }
    // Every pass waits for time, moves to a later step, or ends a completed cut (which consumed
    // the type's wait, or ended the job); a cut changes the tile so it is no candidate any more,
    // so the loop ends (docs/sim/forestry.md 3.2, no_progress_guard).
    for (;;) {
        switch (h.step) {
        case HarvestStep::Start: {
            if (home == nullptr) {
                h.step = HarvestStep::Done; // nothing claimed yet (A: no write, §3.2)
                break;
            }
            if (b.stats != nullptr) {
                ++b.stats->searches;
            }
            const auto found = b.search();
            if (!found) {
                if (b.stats != nullptr) {
                    ++b.stats->searches_failed;
                }
                b.start_home(); // step A fails -> step C (0x1005e280)
                break;
            }
            h.target = found->target;
            b.claim(h.target);
            start_walk(fig.pos, found->path, WalkRef{h.route, h.next, h.along}, &found->end);
            h.step = HarvestStep::ToTree;
            break;
        }
        case HarvestStep::ToTree: {
            const auto rest = walk_ticks(fig.pos, WalkRef{h.route, h.next, h.along}, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            if (home == nullptr) {
                b.abort();
            } else if (b.hygiene && !b.valid(h.target)) {
                b.drop();
            } else {
                // Step A's arrival does nothing (0x1005f710); step B's start waits (0x100553e0).
                h.wait = ft->load_ticks;
                h.step = HarvestStep::Cut;
            }
            break;
        }
        case HarvestStep::Cut: {
            if (ticks < h.wait) {
                h.wait = static_cast<std::uint32_t>(h.wait - ticks);
                return true;
            }
            ticks -= h.wait;
            h.wait = 0;
            if (home == nullptr) {
                b.abort();
                break;
            }
            if (b.hygiene && !b.valid(h.target)) {
                b.drop();
                break;
            }
            // Step B's arrival (0x10054530): clear the claim, then the cut; +256 only when the cut
            // changed the tile (credit follows consumption).
            b.unclaim(h.target);
            const bool ok = b.cut(h.target);
            if (ok) {
                const std::uint64_t sum = std::uint64_t{fig.cargo_88} + kHarvestCredit88;
                fig.cargo_88 = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(sum, std::numeric_limits<std::uint32_t>::max()));
            }
            if (b.stats != nullptr) {
                ++(ok ? b.stats->cuts : b.stats->cuts_failed);
                b.stats->raw_credited_88 += ok ? kHarvestCredit88 : 0U;
            }
            if (!ok && ft->load_ticks == 0) {
                b.start_home(); // A: a failed cut that took no time would repeat forever
            } else if (ft->capacity_88 > fig.cargo_88) {
                h.step = HarvestStep::Start; // type 0xb: capacity > amount -> step A (0x1005c260)
            } else {
                b.start_home();
            }
            break;
        }
        case HarvestStep::ToHome: {
            const auto rest = walk_ticks(fig.pos, WalkRef{h.route, h.next, h.along}, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            // Step C's arrival (0x10057bd0 -> home +0xc0 = 0x1006ee70: raw += amount, u16 add, no
            // clip; here the u32 store with the merge's clamp, docs/sim/forestry.md 2.2).
            if (home != nullptr && fig.cargo_goods != 0 && fig.cargo_88 > 0) {
                // The credit slot (D SmallFarm 0x1006f960): raw 1 when the goods are the home's
                // input goods 1, else raw 0 with no further goods test; a Farm has the one raw
                // store.
                const std::uint32_t slot =
                    harvest_credit_slot(home->payload, *home_type, fig.cargo_goods);
                world.fx.store_deltas.push_back(StoreDelta{home->id, StoreDelta::Store::Input, slot,
                                                           std::int64_t{fig.cargo_88}});
                b.delivered = true;
                if (b.stats != nullptr) {
                    b.stats->raw_delivered_88 += fig.cargo_88;
                }
            }
            h.step = HarvestStep::Done;
            break;
        }
        case HarvestStep::Done:
            // Enhanced: an end path never leaves an owned claim behind (E-i); the arrivals already
            // released it, this covers any other way into Done.
            if (b.hygiene && b.owner(h.target) == fig.id) {
                b.unclaim(h.target);
            }
            if (!b.delivered && fig.cargo_88 > 0 && b.stats != nullptr) {
                b.stats->raw_lost_88 += fig.cargo_88;
            }
            world.end_figure(fig, t); // release of the home counter at the end dispatch
            return true;
        }
    }
}

} // namespace anno::sim
