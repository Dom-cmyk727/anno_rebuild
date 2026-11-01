// The ship-founding transaction (see founding.hpp, docs/sim/ship-founding.md section 16).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/building.hpp>
#include <anno/sim/city_creation.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/figure_veto.hpp>
#include <anno/sim/founding.hpp>
#include <anno/sim/founding_payment.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/territory.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "scheduler_internal.hpp"

namespace anno::sim {

namespace {

constexpr bool contains_player(const std::vector<PlayerId>& set, PlayerId p) {
    return std::binary_search(set.begin(), set.end(), p);
}

void insert_player(std::vector<PlayerId>& set, PlayerId p) {
    const auto at = std::lower_bound(set.begin(), set.end(), p);
    if (at == set.end() || *at != p) {
        set.insert(at, p);
    }
}

/// C truncation toward zero to a multiple of 8 (AnnoWelt 0x10080c9f-0x10080cba).
constexpr std::int64_t floor8(std::int64_t v) {
    return v - v % 8;
}

/// PRODTYPE 33..64, the market block ((p - 1) & ~0x1f) + 1 == 0x21, and the three whose claim is a
/// disc (33..35, docs/re/ship-founding.md 11.2).
constexpr bool is_market_block(std::uint32_t prodtype) {
    return prodtype >= 33 && prodtype <= 64;
}
constexpr bool claims_disc(std::uint32_t prodtype) {
    return prodtype >= 33 && prodtype <= 35;
}

ApplyResult refuse(Reject r) {
    ApplyResult out;
    out.status = r;
    return out;
}

} // namespace

std::pair<std::int64_t, std::int64_t> Founding::reveal_centre(std::int64_t origin_x,
                                                              std::int64_t origin_y,
                                                              std::uint32_t w, std::uint32_t h) {
    return {origin_x + (std::int64_t{w} - 1) / 2, origin_y + (std::int64_t{h} - 1) / 2};
}

bool Founding::object_in_reach(std::int64_t ox, std::int64_t oy, std::int64_t cx, std::int64_t cy,
                               std::uint32_t w, std::uint32_t h, std::uint32_t radius,
                               const std::vector<std::uint32_t>& table) {
    const std::int64_t dy = oy > cy ? std::int64_t{(h - 1) & 1U} + (oy - cy) : cy - oy;
    if (dy > radius || static_cast<std::uint64_t>(dy) >= table.size()) {
        return false;
    }
    const std::int64_t dx = ox > cx ? std::int64_t{(w - 1) & 1U} + (ox - cx) : cx - ox;
    return dx <= static_cast<std::int64_t>(table[static_cast<std::size_t>(dy)]);
}

std::optional<std::uint32_t> Founding::reveal_city(const Island& island,
                                                   std::uint32_t neutral_owner, PlayerId player,
                                                   std::int64_t cx, std::int64_t cy,
                                                   std::uint32_t radius,
                                                   const std::vector<std::uint32_t>& half_width) {
    const TileLayer& t = island.tiles;
    const std::int64_t x0 = t.x0(), y0 = t.y0();
    const std::int64_t w = t.width(), h = t.height();
    const std::int64_t r = radius;
    // Early-out (island word +0xaa has the player's bit): every present city already knows him,
    // which includes an island with no other city. Counted over the island's own ring.
    bool any_lacking = false;
    for (const std::optional<City>& c : island.cities) {
        if (c.has_value() && !contains_player(c->player_mask, player)) {
            any_lacking = true;
        }
    }
    if (!any_lacking) {
        return std::nullopt;
    }
    // The rectangle in island-local tiles (AnnoWelt 0x10080c8b-0x10080d59).
    const std::int64_t xs = std::max<std::int64_t>(0, floor8(cx - r) - x0);
    const std::int64_t f2x = floor8(cx + r + 8);
    const std::int64_t xe = f2x >= x0 + w ? w : f2x - x0;
    const std::int64_t ys = std::max<std::int64_t>(0, floor8(cy - r) - y0);
    const std::int64_t f2y = floor8(cy + r + 8);
    const std::int64_t ye = f2y >= y0 + h ? h : f2y - y0;
    std::optional<std::uint32_t> lowest;
    const std::uint64_t count = t.tile_count();
    for (std::int64_t yl = ys + 2; yl < ye; yl += 8) {
        for (std::int64_t xl = xs + 2; xl < xe; xl += 8) {
            // The quirk: only the lowest-numbered kept city is ever visited, so tracking the
            // minimum is exact. The samples keep the original\'s four offsets per cell; the
            // distance of each is measured from its own tile.
            const auto sample = [&](std::int64_t dxl, std::int64_t dyl) {
                const std::int64_t x = xl + dxl, y = yl + dyl;
                const std::int64_t idx = y * w + x;
                if (idx < 0 || static_cast<std::uint64_t>(idx) >= count) {
                    return;
                }
                const std::uint32_t owner = t.owner(static_cast<std::uint64_t>(idx));
                if (owner == neutral_owner || (lowest.has_value() && *lowest <= owner)) {
                    return;
                }
                const City* city = island.city(CityNo{owner});
                if (city == nullptr || contains_player(city->player_mask, player)) {
                    return;
                }
                // The row/column used by the distance is the sample\'s own (xl + dxl, yl + dyl).
                const std::int64_t dy = std::abs(cy - y0 - y);
                if (r < dy || static_cast<std::uint64_t>(dy) >= half_width.size()) {
                    return;
                }
                const std::int64_t dx = std::abs(cx - x0 - x);
                if (static_cast<std::int64_t>(half_width[static_cast<std::size_t>(dy)]) < dx) {
                    return;
                }
                lowest = owner;
            };
            sample(0, 0);
            sample(4, 0);
            sample(0, 4);
            sample(4, 4);
        }
    }
    return lowest;
}

ApplyResult Founding::apply(World& w, const Command& command, const FoundCmd& f,
                            const InAdvance* in_advance) {
    const SimConfig& cfg = w.config_;
    const TypeRegistry& types = *w.types_;
    const PlayerId me = command.issuer;
    Player* player = w.players_.find(me);
    if (player == nullptr) {
        return refuse(Reject::UnknownIssuer);
    }
    // Human players only: kind 8 (no AI object) and kind 11 (an AI object whose founding-time
    // handlers reach no scan or draw for a fresh city, docs/sim/ship-founding.md 17.4).
    if (player->kind != 8 && player->kind != 11) {
        return refuse(Reject::Unsupported);
    }

    // ---- 2. the ship -----------------------------------------------------------------------
    const veto::Rules vrules;
    const Figure* ship = find_figure(w.figures_, f.ship);
    if (ship == nullptr) {
        return refuse(Reject::UnknownFigure);
    }
    const FigureType* ship_type = types.find_figure_type(ship->kind);
    if (ship_type == nullptr || ship_type->bewohntype != vrules.ship_kind) {
        return refuse(Reject::NotAShip);
    }
    if (ship->owner != me) {
        return refuse(Reject::NotShipOwner);
    }
    if (!veto::selected_ship_state_allows(ship->state_word, vrules)) {
        return refuse(Reject::ShipDisabled);
    }

    // ---- 3. the placement ------------------------------------------------------------------
    const PlacementWire& pw = f.placement;
    const BuildingType* type = pw.type.v == 0 ? nullptr : types.find(pw.type);
    if (type == nullptr) {
        return refuse(Reject::UnknownType);
    }
    if (pw.rotation > 3) {
        return refuse(Reject::BadRotation);
    }
    if (!std::binary_search(w.rules_.founding_types.begin(), w.rules_.founding_types.end(),
                            pw.type.v) ||
        !is_market_block(type->prodtype)) {
        return refuse(Reject::NotAFoundingType);
    }
    const auto [first_w, first_h] = footprint(*type, pw.rotation);
    const site::Origin origin = site::centred_origin(f.click_x, f.click_y, first_w, first_h);
    if (origin.x != pw.x || origin.y != pw.y) {
        return refuse(Reject::BadValue);
    }

    // ---- 4. the island and the click -------------------------------------------------------
    // The island under the origin (the original asks the island table at the origin tile).
    if (origin.x > INT32_MAX || origin.y > INT32_MAX) {
        return refuse(Reject::UnknownIsland);
    }
    Island* island =
        w.island_at(static_cast<std::int32_t>(origin.x), static_cast<std::int32_t>(origin.y));
    if (island == nullptr) {
        return refuse(Reject::UnknownIsland);
    }
    if (island->locked()) {
        return refuse(Reject::IslandLocked);
    }
    const TileLayer& tiles = island->tiles;
    if (!site::footprint_inside(tiles, origin, first_w, first_h)) {
        return refuse(Reject::OutOfIsland);
    }
    if (!site::within_range(f.click_x, f.click_y, ship->pos)) {
        return refuse(Reject::OutOfRange);
    }
    if (site::footprint_ownership(tiles, cfg.neutral_owner, origin, 1, 1) !=
        site::Ownership::AllNeutral) {
        return refuse(Reject::SiteOwned); // the origin tile (a city\'s tile: the takeover path)
    }
    for (const std::optional<City>& c : island->cities) {
        if (c.has_value() && c->owner == me) {
            return refuse(Reject::OwnCityOnIsland);
        }
    }

    // ---- 5. the rotation trials ------------------------------------------------------------
    std::optional<std::uint8_t> accepted;
    std::uint8_t terrain_byte = 0;
    for (const std::uint8_t rot : site::rotation_trials(pw.rotation)) {
        const auto [fw, fh] = footprint(*type, rot);
        if (!site::footprint_inside(tiles, origin, fw, fh)) {
            continue;
        }
        if (site::footprint_ownership(tiles, cfg.neutral_owner, origin, fw, fh) !=
            site::Ownership::AllNeutral) {
            continue;
        }
        const site::Compat compat = site::footprint_occupancy(tiles, types, *type, origin, fw, fh);
        if (compat == site::Compat::Unsupported) {
            return refuse(Reject::Unsupported);
        }
        if (compat == site::Compat::Incompatible) {
            continue;
        }
        const site::TerrainVerdict tv =
            site::coast_height_check(VertexGrid::of(*island), *type, rot, origin);
        if (tv.result == site::TerrainResult::Unsupported ||
            tv.result == site::TerrainResult::Unreadable) {
            return refuse(Reject::Unsupported); // cannot decide: never a permissive default
        }
        if (tv.result == site::TerrainResult::Rejected) {
            continue;
        }
        accepted = rot;
        terrain_byte = tv.result_byte;
        break;
    }
    if (!accepted.has_value()) {
        return refuse(Reject::NoRotationFits);
    }
    const std::uint8_t rotation = *accepted;
    const auto [fw, fh] = footprint(*type, rotation);

    // ---- 6. the figure veto ----------------------------------------------------------------
    {
        const veto::Extents ext = veto::scan_extents(type->width, type->height, rotation);
        const std::int64_t m = vrules.query_margin;
        const std::int64_t gx0 = origin.x - m, gy0 = origin.y - m;
        const std::int64_t gx1 = origin.x + ext.w + m, gy1 = origin.y + ext.h + m;
        std::vector<veto::NearbyFigure> near;
        std::set<FigureId> parents; // figures with a crewed child, built on the first crew question
        bool parents_built = false;
        for (const std::optional<Figure>& slot : w.figures_.list.slots) {
            if (!slot.has_value()) {
                continue;
            }
            const Figure& g = *slot;
            const std::int64_t tx = std::int64_t{g.pos.x} / 256, ty = std::int64_t{g.pos.y} / 256;
            if (tx < gx0 || tx >= gx1 || ty < gy0 || ty >= gy1) {
                continue; // not returned by the query whatever its other facts are
            }
            const FigureType* gt = types.find_figure_type(g.kind);
            if (gt == nullptr) {
                return refuse(Reject::FigureFactsMissing);
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
                if (g.owner != me) {
                    n.relation_war = player->relation(2, g.owner);
                }
                if (const Player* op = w.players_.find(g.owner)) {
                    n.owner_property_1c = op->volk;
                }
            } else {
                n.owner = {veto::Fact::False, PlayerId{}};
            }
            n.range_88 = gt->range_88;
            n.hull_half_88 = gt->hull_half_88; // absent stays absent: the veto then refuses
            if (gt->bewohntype >= vrules.crew_kind_min && gt->bewohntype <= vrules.crew_kind_max) {
                if (!parents_built) {
                    for (const std::optional<Figure>& s2 : w.figures_.list.slots) {
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
        const veto::Result v = veto::nearby_figures(
            me, origin, ext.w, ext.h, std::span<const veto::NearbyFigure>(near), vrules);
        if (v.verdict == veto::Verdict::Vetoed) {
            return refuse(Reject::FigureInTheWay);
        }
        if (v.verdict == veto::Verdict::MissingFact) {
            return refuse(Reject::FigureFactsMissing);
        }
    }

    // ---- 7. payment, city, Kontor, creation, territory, reveal: all prepared ---------------
    auto pay = payment::prepare(ship->holds, player->money, *type);
    if (!pay.ok()) {
        return refuse(Reject::CannotAfford);
    }
    CityRequest creq;
    creq.island = island->index;
    creq.owner = me;
    creq.stamp = static_cast<std::uint16_t>(w.clock_.tick());
    if (!w.rules_.city_names.empty()) {
        creq.names = std::span<const std::string>(w.rules_.city_names);
    }
    auto cplan = CityCreation::prepare(w, creq);
    if (!cplan.ok()) {
        switch (cplan.error()) {
        case CityRefusal::OwnCityOnIsland:
            return refuse(Reject::OwnCityOnIsland);
        case CityRefusal::NoCitySlot:
            return refuse(Reject::NoCitySlot);
        default:
            return refuse(Reject::BadValue);
        }
    }
    // Kind 11 over-approximation (17.4/17.6): a number at or below the allocation cursor may meet a
    // stale AI island record whose scan is not ported.
    if (player->kind == 11 &&
        static_cast<std::int64_t>(cplan.value().number.v) <= island->city_cursor) {
        return refuse(Reject::Unsupported);
    }
    auto kplan = CityCreation::prepare_kontor(w, island->index);
    if (!kplan.ok()) {
        return refuse(Reject::SerialExhausted);
    }
    PlacementRecord rec;
    rec.type = pw.type;
    rec.x = static_cast<std::int32_t>(origin.x);
    rec.y = static_cast<std::int32_t>(origin.y);
    rec.rotation = rotation;
    rec.byte0f = terrain_byte;
    rec.kontor_id = kplan.value().id;
    if (const Reject problem = creation_problem(cfg, types, *island, cfg.layout.lists.size(), rec);
        problem != Reject::None) {
        return refuse(problem);
    }
    const CityNo number = cplan.value().number;
    const std::uint32_t radius = claims_disc(type->prodtype) ? type->action_radius : 0;
    const territory::Footprint fp{origin.x, origin.y, fw, fh};

    // The new city\'s mask: the founder and the players his MILIT row marks 3 (registration,
    // AnnoWelt 0x1008622f-0x1008627f; ascending, as the set is).
    std::vector<PlayerId> mask{me};
    for (const Relation& rel : player->relations) {
        if (rel.milit == 3 && w.players_.find(rel.other) != nullptr) {
            insert_player(mask, rel.other);
        }
    }
    // The shared-visibility audience (an ADAPTATION, docs/sim/shared-visibility.md): the founder
    // and the other members of his group that are players of the world. Ascending, unique.
    std::vector<PlayerId> audience{me};
    for (const std::vector<std::uint32_t>& group : w.rules_.shared_visibility) {
        if (std::binary_search(group.begin(), group.end(), me.v)) {
            for (const std::uint32_t member : group) {
                if (w.players_.find(PlayerId{member}) != nullptr) {
                    insert_player(audience, PlayerId{member});
                }
            }
        }
    }
    for (const PlayerId a : audience) {
        insert_player(mask, a);
    }

    // The reveal, planned on the world as it is: the claim only turns neutral tiles into the new
    // city\'s, whose mask already holds the founder, so no sample it keeps depends on the claim.
    std::optional<std::uint32_t> visited;
    std::vector<std::uint32_t> object_table; // claim stage 2: the radius + 6 table
    std::int64_t centre_x = 0, centre_y = 0;
    {
        const auto [cx, cy] = reveal_centre(origin.x, origin.y, fw, fh);
        centre_x = cx;
        centre_y = cy;
        const std::uint32_t reveal_radius = type->action_radius + 8;
        const auto profile = territory::disc_profile(reveal_radius, reveal_radius);
        if (!profile.has_value()) {
            return refuse(Reject::Unsupported);
        }
        if (claims_disc(type->prodtype)) {
            visited = reveal_city(*island, cfg.neutral_owner, me, cx, cy, reveal_radius, *profile);
            auto table = territory::disc_profile(type->action_radius + 6, type->action_radius + 6);
            if (!table.has_value()) {
                return refuse(Reject::Unsupported);
            }
            object_table = std::move(*table);
        }
    }

    // ---- commit: the one fallible operation first, on a local city -------------------------
    City local = make_city(number, me, cfg);
    const auto key = create_building(cfg, w.rules_, types, *island, local, rec,
                                     static_cast<std::uint32_t>(w.clock_.tick()));
    if (!key.ok()) {
        return refuse(key.error()); // unreachable after creation_problem; refuses unchanged
    }
    // From here on nothing can refuse. Payment:
    player->money = pay.value().money_after;
    {
        Figure* s = find_figure(w.figures_, f.ship);
        s->holds = pay.value().holds;
    }
    // The city (slot, owner, stamp, name, the single name draw), adopting the building index.
    // Inside an advance the name draw goes through the advance's counting source, so the
    // report attributes it to DrawSite::CityName; between advances the generator is used directly.
    DrawSource* source = in_advance != nullptr && in_advance->draws != nullptr
                             ? static_cast<DrawSource*>(in_advance->draws)
                             : nullptr;
    const auto draws_before = source != nullptr
                                  ? static_cast<CountingDraw*>(in_advance->draws)->counts
                                  : std::array<std::uint32_t, kDrawSiteCount>{};
    const auto created = CityCreation::commit(w, cplan.value(), source);
    if (source != nullptr) {
        // Report the draw to the advance like every seam does (Scheduler::merge adds it to
        // SimOutput::draws under its site, DrawSite::CityName).
        Effects fx;
        fx.draws = draw_records_between(draws_before, in_advance->draws->counts);
        Scheduler::merge(w, fx, in_advance->ctx->tick, in_advance->ctx->t, true);
    }
    City& city = *island->cities[created.value().v];
    const auto index_stamp = city.index.stamp; // set by the city commit (BuildingIndex::stamp)
    city.index = std::move(local.index);
    city.index.stamp = index_stamp;
    // Registration: the mask, the island\'s players present.
    city.player_mask = mask;
    for (const PlayerId a : audience) {
        insert_player(island->players_present, a);
    }
    // The Kontor counter (the building already holds the old value).
    static_cast<void>(CityCreation::commit_kontor(w, kplan.value()));
    // Territory: the disc for the market block 33..35, then the footprint, neutral tiles only.
    if (claims_disc(type->prodtype)) {
        static_cast<void>(territory::claim_neutral_disc(island->tiles, cfg.neutral_owner, number.v,
                                                        fp, radius, radius));
    }
    static_cast<void>(
        territory::claim_neutral_rect(island->tiles, cfg.neutral_owner, number.v, fp));
    // Claim stage 2: the founder discovers the ore objects within reach (idempotent per player).
    if (claims_disc(type->prodtype)) {
        for (OreObject& o : island->ore_objects) {
            if (object_in_reach(o.x, o.y, centre_x, centre_y, fw, fh, radius, object_table)) {
                for (const PlayerId a : audience) {
                    insert_player(o.discovered, a);
                }
            }
        }
    }
    // The reveal\'s single visit.
    if (visited.has_value()) {
        City* other = island->city(CityNo{*visited});
        for (const PlayerId a : audience) {
            insert_player(other->player_mask, a);
            if (Player* ap = w.players_.find(a)) {
                insert_player(ap->discovery, other->owner);
            }
        }
    }
    // The island recount (0x134(1)): every player in a city\'s mask learns that city\'s owner.
    for (const std::optional<City>& c : island->cities) {
        if (!c.has_value()) {
            continue;
        }
        for (const PlayerId known : c->player_mask) {
            if (Player* kp = w.players_.find(known)) {
                insert_player(kp->discovery, c->owner);
            }
        }
    }

    ApplyResult out;
    RecordResult rr;
    rr.building = island->city(number)->index.lists[key.value().list].slots[key.value().slot]->id;
    out.records.push_back(rr);
    return out;
}

} // namespace anno::sim
