#include <algorithm>
#include <set>

#include <anno/sim/figure_veto.hpp>
#include <anno/sim/road_drag.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

std::vector<TileXY> road_path(TileXY start, TileXY end, std::uint32_t cap) {
    std::vector<TileXY> out;
    if (cap == 0) {
        return out;
    }
    const std::int64_t dx = std::int64_t{end.x} - std::int64_t{start.x};
    const std::int64_t dy = std::int64_t{end.y} - std::int64_t{start.y};
    const std::int64_t adx = dx < 0 ? -dx : dx;
    const std::int64_t ady = dy < 0 ? -dy : dy;
    const std::int32_t sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    const std::int32_t sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);

    const std::uint64_t total =
        std::min<std::uint64_t>(cap, static_cast<std::uint64_t>(adx + ady) + 1U);
    out.reserve(static_cast<std::size_t>(total));

    std::int32_t cx = start.x;
    std::int32_t cy = start.y;
    out.push_back({cx, cy});

    const bool x_first = adx > ady; // tie (adx == ady) runs y first (AnnoFrame 0x1004b392)
    const std::int64_t first_steps = x_first ? adx : ady;
    const std::int64_t second_steps = x_first ? ady : adx;

    for (std::int64_t i = 0; i < first_steps && out.size() < cap; ++i) {
        if (x_first) {
            cx += sx;
        } else {
            cy += sy;
        }
        out.push_back({cx, cy});
    }
    for (std::int64_t i = 0; i < second_steps && out.size() < cap; ++i) {
        if (x_first) {
            cy += sy;
        } else {
            cx += sx;
        }
        out.push_back({cx, cy});
    }
    return out;
}

Reject check_road_figure_veto(const World& w, PlayerId placing, std::int64_t x, std::int64_t y,
                              std::uint32_t fw, std::uint32_t fh) {
    const Player* player = w.players().find(placing);
    if (player == nullptr) {
        return Reject::UnknownIssuer;
    }
    const veto::Rules vrules;
    const std::int64_t m = vrules.query_margin;
    const std::int64_t gx0 = x - m;
    const std::int64_t gy0 = y - m;
    const std::int64_t gx1 = x + std::int64_t{fw} + m;
    const std::int64_t gy1 = y + std::int64_t{fh} + m;

    std::vector<veto::NearbyFigure> near;
    std::set<FigureId> parents;
    bool parents_built = false;

    for (const std::optional<Figure>& slot : w.figures().list.slots) {
        if (!slot.has_value()) {
            continue;
        }
        const Figure& g = *slot;
        const std::int64_t tx = std::int64_t{g.pos.x} / 256;
        const std::int64_t ty = std::int64_t{g.pos.y} / 256;
        if (tx < gx0 || tx >= gx1 || ty < gy0 || ty >= gy1) {
            continue;
        }
        const FigureType* gt = w.types().find_figure_type(g.kind);
        if (gt == nullptr) {
            return Reject::FigureFactsMissing;
        }
        veto::NearbyFigure n;
        n.id = g.id;
        n.registration =
            g.parent.valid() ? veto::Registration::Embarked : veto::Registration::SectorCell;
        n.pos = g.pos;
        n.kind = gt->bewohntype;
        n.state_word = g.state_word;
        if (g.owner.valid()) {
            n.owner = {veto::Fact::True, g.owner};
            if (g.owner != placing) {
                n.relation_war = player->relation(2, g.owner);
            }
            if (const Player* op = w.players().find(g.owner)) {
                n.owner_property_1c = op->volk;
            }
        } else {
            n.owner = {veto::Fact::False, PlayerId{}};
        }
        n.range_88 = gt->range_88;
        n.hull_half_88 = gt->hull_half_88;
        if (gt->bewohntype >= vrules.crew_kind_min && gt->bewohntype <= vrules.crew_kind_max) {
            if (!parents_built) {
                for (const std::optional<Figure>& s2 : w.figures().list.slots) {
                    if (s2.has_value() && s2->parent.valid() && s2->crew_slot.has_value()) {
                        parents.insert(s2->parent);
                    }
                }
                parents_built = true;
            }
            n.crew_occupied = parents.count(g.id) != 0 ? veto::Fact::True : veto::Fact::False;
        }
        near.push_back(n);
    }

    const veto::Result v = veto::nearby_figures(placing, site::Origin{x, y}, fw, fh,
                                                std::span<const veto::NearbyFigure>(near), vrules);
    if (v.verdict == veto::Verdict::Vetoed) {
        return Reject::FigureInTheWay;
    }
    if (v.verdict == veto::Verdict::MissingFact) {
        return Reject::FigureFactsMissing;
    }
    return Reject::None;
}

BuildCmd make_road_drag_cmd(CityRef city, TypeId road_type, TileXY start, TileXY end,
                            std::uint8_t rotation, std::uint32_t cap) {
    BuildCmd cmd;
    cmd.city = city;
    const std::vector<TileXY> path = road_path(start, end, cap);
    cmd.placements.reserve(path.size());
    for (const TileXY& tile : path) {
        PlacementWire p;
        p.type = road_type;
        p.x = tile.x;
        p.y = tile.y;
        p.rotation = rotation;
        cmd.placements.push_back(p);
    }
    return cmd;
}

} // namespace anno::sim
