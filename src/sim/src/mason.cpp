// The stonemason's worker job 0x2c (MAP-EXTRACT-2 part Q). Integer only; no draw.
// Contract: docs/sim/quarry-bricks.md (Q1.2, F1 to F4, review corrections, DECIDED Q-A, Q3).
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

#include <anno/sim/driver.hpp>
#include <anno/sim/inn.hpp>
#include <anno/sim/mason.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/walk_search.hpp>

namespace anno::sim {

std::optional<std::int32_t> mason_priority(std::uint32_t raw, std::uint32_t need) {
    const auto p = base_priority(raw, need);
    if (!p || *p < 0) {
        return std::nullopt; // the base priority answers -1 at 60 steps of stock: no candidate
    }
    return p;
}

std::optional<std::int64_t> mason_candidate_score(std::int32_t priority, std::int64_t stock,
                                                  std::int64_t capacity) {
    if (capacity <= 0) {
        return std::nullopt;
    }
    return std::int64_t{priority} + stock * 3 * 64 / capacity;
}

namespace {

GoodsId goods_at(const std::vector<GoodsId>& v, std::size_t i) {
    return i < v.size() ? v[i] : GoodsId{0};
}

/// The building at a world tile through the mask-8 lists of any city of the island (D 0x100543b0:
/// the island's point lookup), or nullptr.
const Building* building_at_tile(const Island& island, const ListLayout& layout, TileXY t) {
    for (std::size_t n = 0; n < island.cities.size(); ++n) {
        if (const Building* b = island.building_at(layout, CityNo{static_cast<std::uint32_t>(n)},
                                                   t.x, t.y, kMasonMask)) {
            return b;
        }
    }
    return nullptr;
}

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
    bool delivered = false;

    void claim(bool set) {
        world.fx.quarry_claims.push_back(
            QuarryClaim{island_id, h.target, h.goal_col, h.goal_row, set});
        if (stats != nullptr) {
            ++(set ? stats->quarry_claims_set : stats->quarry_claims_cleared);
        }
    }
    void reset_walk() {
        h.route.clear();
        h.next = 0;
        h.along = 0;
    }
    /// The abort (the home is gone at an arrival, D 0x10055480 with mode 1 on the building found
    /// at the stored tile), then Done.
    void abort() {
        claim(false);
        h.wait = 0;
        reset_walk();
        h.step = HarvestStep::Done;
        if (stats != nullptr) {
            ++stats->aborts;
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
        WalkPoint end;
        const auto path = path_home(*island, ctx.types, *home, *home_type, fig.pos, ft.layer, &end);
        if (!path) {
            h.step = HarvestStep::Done;
            return;
        }
        start_walk(fig.pos, *path, WalkRef{h.route, h.next, h.along}, &end);
    }

    struct Found {
        WalkPoint goal;
        std::vector<TileXY> path;
    };
    /// Step A's search (D 0x10059bf0 + the callback 0x1005a400 with threshold 0): the candidates of
    /// the mask-8 list in the home's square, every unclaimed entrance of every candidate a goal,
    /// the FIRST goal popped stops the search (its building scores above the best of 0).
    [[nodiscard]] std::optional<Found> search() const {
        if (island == nullptr || home == nullptr || home_type == nullptr ||
            home_type->input_goods.empty()) {
            return std::nullopt;
        }
        const GoodsId wanted = home_type->input_goods[0];
        const auto* farm = std::get_if<Farm>(&home->payload);
        const std::uint32_t need =
            home_type->prod.input_per_step.empty() ? 0U : home_type->prod.input_per_step[0];
        const auto prio = mason_priority(farm != nullptr ? farm->raw : 0U, need);
        if (!prio) {
            return std::nullopt; // the need gate: 60 steps of stock or more (or a need of 0)
        }
        const SearchArea area = search_area(*home, *home_type);
        const std::int64_t sx = std::int64_t{area.cx0} - area.radius;
        const std::int64_t sy = std::int64_t{area.cy0} - area.radius;
        if (sx < 0 || sy < 0) {
            return std::nullopt; // the u16 origin wraps beyond the island (as the harvest job's)
        }
        const auto cref = [&]() -> std::optional<CityRef> {
            const auto it = island->cache.by_id.find(home->id);
            return it == island->cache.by_id.end() ? std::nullopt
                                                   : std::optional<CityRef>(it->second.city);
        }();
        if (!cref) {
            return std::nullopt;
        }
        const City* city = island->city(cref->city);
        std::vector<BuildingId> ids;
        world.rect_query(*cref, area.square(), kMasonMask, ids);
        struct Candidate {
            const Building* b;
            const BuildingType* t;
            bool scored = false;
        };
        std::vector<Candidate> candidates;
        for (const BuildingId id : ids) {
            const Building* b = world.find_building(id);
            const BuildingType* t = b != nullptr ? ctx.types.find(b->type) : nullptr;
            if (t == nullptr || b->id == home->id) {
                continue;
            }
            // The output must be the mason's input (line 0 or 1; the "ALL" output 0x8e is a
            // market's and markets are outside the mask-8 list).
            if (goods_at(t->output_goods, 0) != wanted && goods_at(t->output_goods, 1) != wanted) {
                continue;
            }
            const std::int64_t stock =
                std::holds_alternative<Quarry>(b->payload) || t->cls == RuntimeClass::Quarry
                    ? std::int64_t{kQuarryStock88}
                    : available(*b, *t, wanted, city);
            if (stock < std::int64_t{kQuarryStock88}) {
                continue;
            }
            const std::int64_t cap =
                t->has_prod && !t->prod.capacity.empty() ? std::int64_t{t->prod.capacity[0]} : 0;
            if (const auto score = mason_candidate_score(*prio, stock, cap);
                !score || *score <= 0) {
                continue;
            }
            candidates.push_back(Candidate{b, t});
        }
        if (candidates.empty()) {
            return std::nullopt;
        }
        const auto& tables = ctx.types.walk_tables();
        if (!tables) {
            return std::nullopt;
        }
        struct Goal {
            WalkPoint point;
            std::size_t candidate;
        };
        std::vector<Goal> mapped;
        std::vector<WalkPoint> goals;
        for (std::size_t k = 0; k < candidates.size(); ++k) {
            const auto* quarry = std::get_if<Quarry>(&candidates[k].b->payload);
            const std::vector<WalkPoint> points =
                entrance_walk_points(*candidates[k].b, *candidates[k].t);
            for (std::size_t i = 0; i < points.size(); ++i) {
                // D 0x10070994: an entrance whose claim bit is set is not registered.
                if (quarry != nullptr && i < quarry->claims.size() && quarry->claims[i]) {
                    continue;
                }
                const WalkPoint p = points[i];
                if (std::find_if(mapped.begin(), mapped.end(),
                                 [&](const Goal& g) { return g.point == p; }) == mapped.end()) {
                    mapped.push_back(Goal{p, k});
                    goals.push_back(p);
                }
            }
        }
        if (goals.empty()) {
            return std::nullopt;
        }
        WalkCells cells(*island, ctx.types, *tables, area.square(), 4, area);
        const std::array starts{walk_point_of(fig.pos)};
        bool chosen = false;
        const SearchResult found =
            anno::sim::search(cells, *tables, ft.layer, starts, goals, [&](const SearchVisit& v) {
                const auto g = std::find_if(mapped.begin(), mapped.end(),
                                            [&](const Goal& x) { return x.point == v.point; });
                if (g == mapped.end() || chosen) {
                    return SearchDecision{};
                }
                // The best score starts at 0 and the first scored candidate exceeds it; the
                // context threshold 0 is below the stock, so the search STOPS here (D 0x1005a4a3).
                chosen = true;
                return SearchDecision{GoalAction::Stop, true};
            });
        if (stats != nullptr) {
            stats->search_popped += found.popped;
        }
        if (!found.complete || !found.selected || !chosen || found.route.empty()) {
            return std::nullopt;
        }
        return Found{found.selected->point, found.route};
    }
};

} // namespace

bool mason_tick(const TickContext& ctx, SerialWorld& world, Figure& fig) {
    const FigureType* ft = ctx.types.find_figure_type(fig.kind);
    if (ft == nullptr || !fig.harvest.has_value()) {
        return false;
    }
    const std::uint32_t t = ctx.t;
    HarvestState& h = *fig.harvest;
    if (fig.expiry_stamp.has_value()) {
        fig.stamp = static_cast<std::uint16_t>(t); // ended, waiting for phase C
        return true;
    }
    std::uint64_t ticks = static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - fig.stamp);
    if (ticks == 0) {
        return true;
    }
    fig.stamp = static_cast<std::uint16_t>(t);
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
    if (home != nullptr && !std::holds_alternative<Farm>(home->payload)) {
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
           ctx.transport_stats != nullptr ? &ctx.transport_stats->forestry : nullptr};
    const std::uint32_t speed = ft->speed_q24;
    for (;;) {
        switch (h.step) {
        case HarvestStep::Start: {
            if (home == nullptr) {
                h.step = HarvestStep::Done; // nothing claimed yet
                break;
            }
            if (b.stats != nullptr) {
                ++b.stats->searches;
                ++b.stats->mason_searches[home->id];
            }
            const auto found = b.search();
            if (!found) {
                if (b.stats != nullptr) {
                    ++b.stats->searches_failed;
                    ++b.stats->mason_searches_failed[home->id];
                }
                b.start_home(); // step A fails -> step C with an empty hand (D 0x1005d380)
                break;
            }
            h.target = found->goal.tile;
            h.goal_col = static_cast<std::uint8_t>(found->goal.col);
            h.goal_row = static_cast<std::uint8_t>(found->goal.row);
            b.claim(true); // D 0x10059dfe: +0xfc mode 0 on the chosen building
            start_walk(fig.pos, found->path, WalkRef{h.route, h.next, h.along}, &found->goal);
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
                b.abort(); // D 0x1005d513: home missing at the arrival
            } else {
                h.wait = ft->load_ticks; // step B's start (0x100553e0): the wait of the type
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
            // Step B's arrival (0x10055420): the building at the stored tile (mask 8). Found:
            // +0xfc mode 1 clears the claim bit, and the cargo gains 1 t once. Gone: the cargo is
            // unchanged and the job goes on.
            const Building* at = b.island != nullptr
                                     ? building_at_tile(*b.island, ctx.cfg.layout, h.target)
                                     : nullptr;
            b.claim(false);
            if (at != nullptr) {
                const std::uint64_t sum = std::uint64_t{fig.cargo_88} + kMasonTripCredit88;
                fig.cargo_88 = static_cast<std::uint32_t>(
                    std::min<std::uint64_t>(sum, std::numeric_limits<std::uint32_t>::max()));
                if (b.stats != nullptr) {
                    b.stats->quarry_lifted_88[at->id] += kMasonTripCredit88;
                    b.stats->raw_credited_88 += kMasonTripCredit88;
                }
            }
            b.start_home();
            break;
        }
        case HarvestStep::ToHome: {
            const auto rest = walk_ticks(fig.pos, WalkRef{h.route, h.next, h.along}, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            // Step C's arrival (0x10057bd0 -> 0x1006ee70): raw += cargo, unconditional.
            if (home != nullptr && fig.cargo_goods != 0 && fig.cargo_88 > 0) {
                world.fx.store_deltas.push_back(
                    StoreDelta{home->id, StoreDelta::Store::Input, 0, std::int64_t{fig.cargo_88}});
                b.delivered = true;
                if (b.stats != nullptr) {
                    b.stats->raw_delivered_88 += fig.cargo_88;
                    b.stats->mason_delivered_88[home->id] += fig.cargo_88;
                }
            }
            h.step = HarvestStep::Done;
            break;
        }
        case HarvestStep::Done:
            if (!b.delivered && fig.cargo_88 > 0 && b.stats != nullptr) {
                b.stats->raw_lost_88 += fig.cargo_88;
                b.stats->mason_lost_88[fig.home] += fig.cargo_88;
            }
            world.end_figure(fig, t);
            return true;
        }
    }
}

} // namespace anno::sim
