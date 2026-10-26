// World::apply: commands applied between advances (docs/sim/state.md §8). Stage S8
// implements SetSpeed and SetProductionLine; stage S10 Build, Demolish, SetTradeFlag
// and SetTradeSlot (§8.4, §15 S10). A legacy record is never executed (fail closed).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <variant>
#include <vector>

#include <anno/sim/apply.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "scheduler_internal.hpp"

namespace anno::sim {

namespace {

/// What the command functions read and write, handed down by Commands::apply (the
/// friend of World). `tick` is the clock's tick: between two advances the tick of the
/// last advance, inside one the tick being advanced to.
struct Env {
    World& w;
    const SimConfig& cfg;
    const Rules& rules;
    const WorldSettings& settings;
    const Seams& seams;
    const TypeRegistry& types;
    PlayerTable& players;
    std::vector<std::optional<Island>>& islands;
    WorldRng& rng;
    std::uint64_t tick;
    const InAdvance* in_advance;
};

/// Switches the line of output `index` of a payload. False when the payload has no
/// switch for that output (a class without one, or an index beyond its lines).
bool switch_line(Payload& payload, std::size_t index, bool on) {
    struct Switch {
        std::size_t index;
        bool on;
        bool operator()(Workshop& w) const {
            w.switched_on = on;
            return true;
        }
        bool operator()(Farm& f) const {
            f.switched_on = on;
            return true;
        }
        bool operator()(Mine& m) const {
            m.switched_on = on;
            return true;
        }
        bool operator()(WeaponsWorkshop& w) const {
            if (index >= w.line_on.size()) {
                return false;
            }
            w.line_on[index] = on;
            return true;
        }
        bool operator()(SmallFarm& s) const {
            if (index >= s.line_on.size()) {
                return false;
            }
            s.line_on[index] = on;
            return true;
        }
        bool operator()(Inert&) const { return false; }
        bool operator()(Market&) const { return false; }
    };
    return std::visit(Switch{index, on}, payload);
}

Reject set_speed(const SimConfig& cfg, WorldSettings& settings, const Command& c,
                 const SetSpeedCmd& s) {
    if (c.issuer != cfg.host_player) {
        return Reject::NotHost;
    }
    if (!is_original_speed(s.percent, cfg.multiplayer)) {
        return Reject::BadSpeed;
    }
    settings.speed_percent = s.percent;
    return Reject::None;
}

Reject set_production_line(World& w, const SimConfig& cfg, const TypeRegistry& types,
                           const Command& c, const SetProductionLineCmd& s) {
    Island* is = w.island_at(s.tile.x, s.tile.y);
    if (is == nullptr) {
        return Reject::UnknownIsland;
    }
    const Slot idx = is->tile_index(s.tile.x, s.tile.y);
    const std::uint32_t owner = is->tiles.owner(idx);
    const City* city = owner == cfg.neutral_owner ? nullptr : is->city(CityNo{owner});
    if (city == nullptr) {
        return Reject::UnknownCity;
    }
    if (city->owner != c.issuer) {
        return Reject::NotCityOwner;
    }
    const Building* found = is->building_at(cfg.layout, city->number, s.tile.x, s.tile.y);
    if (found == nullptr) {
        return Reject::NoBuildingAtTile;
    }
    const BuildingType* type = types.find(found->type);
    if (type == nullptr) {
        return Reject::UnknownType;
    }
    const auto it = std::find(type->output_goods.begin(), type->output_goods.end(), s.goods);
    if (it == type->output_goods.end()) {
        return Reject::LineNotFound;
    }
    Building* b = find_building_mut(*is, found->id);
    if (b == nullptr ||
        !switch_line(b->payload, static_cast<std::size_t>(it - type->output_goods.begin()), s.on)) {
        return Reject::LineNotFound;
    }
    return Reject::None;
}

// ---- city-scoped commands ----------------------------------------------------------

/// The island and city a city-scoped command names, or why not (§8.2 item 3, F9, F11):
/// the island slot must hold an island (UnknownIsland), the city slot a city
/// (UnknownCity), and the city must be the issuer's (NotCityOwner).
struct Target {
    Island* island = nullptr;
    City* city = nullptr;
    Reject status = Reject::None;
};

Target resolve_city(Env& env, const Command& c, CityRef ref) {
    Target t;
    if (!ref.island.valid() || ref.island.v >= env.islands.size() ||
        !env.islands[ref.island.v].has_value()) {
        t.status = Reject::UnknownIsland;
        return t;
    }
    Island& is = *env.islands[ref.island.v];
    City* city = is.city(ref.city);
    if (city == nullptr) {
        t.status = Reject::UnknownCity;
        return t;
    }
    if (city->owner != c.issuer) {
        t.status = Reject::NotCityOwner;
        return t;
    }
    t.island = &is;
    t.city = city;
    return t;
}

/// Runs a Place or Remove seam (Seams::on_placed / on_removed, O-4, O-5) on `b` and
/// merges its Effects at once (C2). Inside an advance the seam gets the advance's
/// context and draw source; between two advances a context at the clock's tick over
/// the world PRNG (its draws are the seam's, like those of every seam). A seam without
/// a function is the default: nothing.
void run_building_seam(Env& env, const Seam<Seams::PlaceHook>& seam, Island& is, City& city,
                       Building& b) {
    if (seam.fn == nullptr) {
        return;
    }
    Effects fx;
    if (env.in_advance != nullptr && env.in_advance->ctx != nullptr &&
        env.in_advance->draws != nullptr) {
        const TickContext& ctx = *env.in_advance->ctx;
        const auto before = env.in_advance->draws->counts;
        seam.fn(seam.ctx, ctx, is, city, b, fx);
        fx.draws = draw_records_between(before, env.in_advance->draws->counts);
        Scheduler::merge(env.w, fx, ctx.tick, ctx.t, true);
        return;
    }
    GlobalDraw global(env.rng);
    CountingDraw draw(global);
    const auto t = static_cast<std::uint32_t>(env.tick);
    const TickContext ctx{env.tick,
                          t,
                          env.cfg,
                          env.rules,
                          env.settings,
                          env.types,
                          PlayersView(env.players),
                          IslandsView(env.islands),
                          draw,
                          env.seams};
    seam.fn(seam.ctx, ctx, is, city, b, fx);
    Scheduler::merge(env.w, fx, env.tick, t, false);
}

// ---- Build 0x64 (§8.4) -------------------------------------------------------------

/// Whether any building of the island (any city, any list) covers tile `idx`.
bool tile_has_building(const Island& is, std::uint64_t idx) {
    for (const std::vector<std::uint32_t>& grid : is.cache.grids) {
        if (idx < grid.size() && grid[static_cast<std::size_t>(idx)] != 0) {
            return true;
        }
    }
    return false;
}

/// The scratch budget of the atomic dry run (Rules::build_atomic, D8, §8.2 item 5): what
/// the records before the current one would take if they were placed.
struct Budget {
    /// Gold of the earlier records.
    std::int64_t money = 0;
    /// Goods of the earlier records, by goods number.
    std::map<GoodsId, std::uint64_t> goods;
    /// Serials of the earlier records.
    std::uint64_t serials = 0;
    /// Island tile indices the earlier records cover.
    std::set<std::uint64_t> tiles;
};

/// One record after validation: the record to create (after an upgrade rewrite), its
/// type, and whether the cost is waived.
struct Checked {
    Reject status = Reject::None;
    /// Type id 0: the record is skipped (D: op 6 skips it, §8.4 step 1).
    bool skip = false;
    const BuildingType* type = nullptr;
    PlacementRecord rec;
    bool free = false;
};

/// The island tile indices of a footprint that lies on the island.
template <class F>
void for_each_footprint_tile(const Island& is, const BuildingType& type, const PlacementRecord& rec,
                             F&& f) {
    const auto [fw, fh] = footprint(type, rec.rotation);
    const TileLayer& tiles = is.tiles;
    const TileBox box =
        clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw, fh);
    for (std::int64_t y = box.y0; y < box.y1; ++y) {
        for (std::int64_t x = box.x0; x < box.x1; ++x) {
            f(static_cast<std::uint64_t>(y * tiles.width() + x));
        }
    }
}

bool same_record(const PlacementRecord& a, const PlacementRecord& b) {
    return a.type == b.type && a.x == b.x && a.y == b.y && a.rotation == b.rotation &&
           a.byte0f == b.byte0f;
}

/// Steps 1-6 of §8.4 for one record, read-only. With `budget` (the atomic dry run) the
/// earlier records of the command count as placed: their gold, goods and serials are
/// spent and their tiles taken.
Checked check_record(const Env& env, const Island& is, const City& city, const Player& owner,
                     const PlacementWire& p, const Budget* budget) {
    Checked out;
    // 1. Type id 0 is skipped (D, §8.4).
    if (p.type.v == 0) {
        out.skip = true;
        return out;
    }
    // byte0f of a command-created building is 0: the original overwrites rec[8] from the
    // terrain height check (0x1007c790), so the client's byte8 is not used (§8.4).
    PlacementRecord rec{p.type, p.x, p.y, p.rotation, 0};
    const BuildingType* type = nullptr;
    // A rewrite by Seams::upgrade_rewrite (step 4) is validated again from step 1, once.
    for (bool rewritten = false;;) {
        type = env.types.find(rec.type);
        if (type == nullptr) {
            out.status = Reject::UnknownType; // F4
            return out;
        }
        if (rec.rotation > 3) {
            out.status = Reject::BadRotation; // F4
            return out;
        }
        // PRODTYPE 0 or a number without a list (roads, walls, scenery: tile objects,
        // D: Place skips Create for the blocks above 0xa0) is not ported (O-4).
        if (!env.cfg.layout.list_for(type->prodtype).has_value()) {
            out.status = Reject::Unsupported;
            return out;
        }
        // 2. The footprint on the island, on the city's tiles (D 0x10058890).
        if (const Reject r = footprint_owner_rule(is, env.cfg, *type, rec, city.number);
            r != Reject::None) {
            out.status = r;
            return out;
        }
        // 3. The placement rule (D 0x1007c790, O-27; default: the D17 stand-in).
        const Reject placed =
            env.seams.placement_rule.fn != nullptr
                ? env.seams.placement_rule.fn(env.seams.placement_rule.ctx, is, *type, rec)
                : stand_in_placement_rule(is, *type, rec);
        if (placed != Reject::None) {
            out.status = placed;
            return out;
        }
        // create_building's own overlap check (F32), here so that a validated record
        // cannot fail at its creation (a placement seam may ignore buildings), and in the
        // atomic dry run the tiles of the earlier records of the command.
        bool taken = false;
        for_each_footprint_tile(is, *type, rec, [&](std::uint64_t idx) {
            taken = taken || tile_has_building(is, idx) ||
                    (budget != nullptr && budget->tiles.count(idx) != 0);
        });
        if (taken) {
            out.status = Reject::Occupied;
            return out;
        }
        // 4. The upgrade rewrite (D 0x100276a0 for PRODTYPE 0x43 / 0x48, O-28; default
        // none).
        if (!rewritten && env.seams.upgrade_rewrite.fn != nullptr) {
            PlacementRecord changed = rec;
            env.seams.upgrade_rewrite.fn(env.seams.upgrade_rewrite.ctx, *type, changed);
            rewritten = true;
            if (!same_record(changed, rec)) {
                rec = changed;
                continue;
            }
        }
        break;
    }
    // 5. Research (Rules::enforce_research, O-30; default off: the original has no
    // sim-side research gate on Build).
    if (env.rules.enforce_research && type->research_id != 0 &&
        !owner.research.researched(type->research_id)) {
        out.status = Reject::NotResearched;
        return out;
    }
    // The serial the creation takes (create_building refuses 0 and 0xffffffff).
    const std::uint64_t serials_before = budget != nullptr ? budget->serials : 0;
    if (is.next_serial == 0 || std::uint64_t{is.next_serial} + serials_before >= 0xffffffffU) {
        out.status = Reject::SerialExhausted;
        return out;
    }
    // 6. Affordability, read-only (D 0x100275d0), against the state the earlier records
    // left (D8). Waived by the owner's free-build flag (D owner slot +0x9c(7), O-9) and
    // by Rules::free_build_cheat (F13: the original's free-build flag 0x2000000 came from
    // the payload; here it is a hashed session rule, never a wire field; OQ-125). A waiver
    // skips the check and the charge (I: "cost waiver"; OQ-126).
    out.free = env.rules.free_build_cheat || owner.free_build;
    if (!out.free) {
        const std::int64_t gold_before = budget != nullptr ? budget->money : 0;
        if (owner.money < gold_before + std::int64_t{type->cost_gold}) {
            out.status = Reject::CannotAfford;
            return out;
        }
        // The goods per goods number: a type may name one goods twice (the registry does
        // not refuse it), and the sum must be in stock.
        std::map<GoodsId, std::uint64_t> need;
        for (const CostEntry& cost : type->cost_goods) {
            // A cost goods outside the charged goods (validate_against refuses such a
            // registry) or the warehouse cannot be paid.
            if (cost.goods < env.cfg.build_goods_first ||
                cost.goods - env.cfg.build_goods_first >= city.build_goods.size() ||
                cost.goods >= city.stock.stock.size()) {
                out.status = Reject::CannotAfford;
                return out;
            }
            need[cost.goods] += cost.amount_88;
        }
        for (const auto& [goods, amount] : need) {
            std::uint64_t used = 0;
            if (budget != nullptr) {
                const auto it = budget->goods.find(goods);
                used = it != budget->goods.end() ? it->second : 0;
            }
            if (used + amount > city.stock.stock[goods]) {
                out.status = Reject::CannotAfford;
                return out;
            }
        }
    }
    out.type = type;
    out.rec = rec;
    return out;
}

/// Adds a validated record to the dry run's budget.
void spend(Budget& budget, const Island& is, const Checked& c) {
    if (!c.free) {
        budget.money += std::int64_t{c.type->cost_gold};
        for (const CostEntry& cost : c.type->cost_goods) {
            budget.goods[cost.goods] += cost.amount_88;
        }
    }
    ++budget.serials;
    for_each_footprint_tile(is, *c.type, c.rec,
                            [&](std::uint64_t idx) { budget.tiles.insert(idx); });
}

/// Step 7 (D 0x100279a0): create, charge, run the Place seam. The original charges
/// before Create and does not refund when Create fails (F18); here nothing reads the
/// money or the stock between the two, so charging after a successful creation is the
/// same state and leaves nothing to refund when the creation is refused (ListFull, a
/// list of 2^32 slots; or a placement seam that let an overlap through: create_building
/// refuses it, F32).
RecordResult place(Env& env, Island& is, City& city, Player& owner, const Checked& c) {
    RecordResult r;
    const auto key = create_building(env.cfg, env.rules, env.types, is, city, c.rec,
                                     static_cast<std::uint32_t>(env.tick));
    if (!key.ok()) {
        r.status = key.error();
        return r;
    }
    if (!c.free) {
        owner.money -= std::int64_t{c.type->cost_gold}; // >= cost, checked in step 6
        for (const CostEntry& cost : c.type->cost_goods) {
            city.build_goods[cost.goods - env.cfg.build_goods_first].spent += cost.amount_88;
            static_cast<void>(city.stock.remove(cost.goods, cost.amount_88));
        }
    }
    Building& b = *city.index.lists[key.value().list].slots[key.value().slot];
    r.building = b.id;
    run_building_seam(env, env.seams.on_placed, is, city, b);
    return r;
}

ApplyResult build(Env& env, const Command& c, const BuildCmd& cmd) {
    ApplyResult result;
    const Target t = resolve_city(env, c, cmd.city);
    if (t.status != Reject::None) {
        result.status = t.status;
        return result;
    }
    Island& is = *t.island;
    City& city = *t.city;
    Player* owner = env.players.find(city.owner); // the issuer, who exists
    if (owner == nullptr) {
        result.status = Reject::UnknownIssuer;
        return result;
    }
    result.records.resize(cmd.placements.size());

    // Rules::build_atomic (D8): every record is validated against a scratch budget
    // first; one failure places nothing. The records that would have passed say
    // AtomicAbort, the failing ones their own reason.
    if (env.rules.build_atomic) {
        Budget budget;
        bool all_pass = true;
        for (std::size_t i = 0; i < cmd.placements.size(); ++i) {
            const Checked chk = check_record(env, is, city, *owner, cmd.placements[i], &budget);
            if (chk.skip) {
                continue;
            }
            if (chk.status != Reject::None) {
                result.records[i].status = chk.status;
                all_pass = false;
                continue;
            }
            spend(budget, is, chk);
        }
        if (!all_pass) {
            for (std::size_t i = 0; i < cmd.placements.size(); ++i) {
                if (result.records[i].status == Reject::None && cmd.placements[i].type.v != 0) {
                    result.records[i].status = Reject::AtomicAbort;
                }
            }
            result.status = Reject::AtomicAbort;
            return result;
        }
    }

    // Record by record in stream order (§8.2 item 5; D: op 6 continues after a failed
    // record, 0x10058a6d), each validated against the state the earlier ones left. Under
    // build_atomic every record passed the dry run; the live check repeats it (only a
    // Place seam's Effects could make it differ: the dry run repeats create_building's
    // checks, so a validated record is created).
    bool placed_any = false;
    Reject first_failure = Reject::None;
    for (std::size_t i = 0; i < cmd.placements.size(); ++i) {
        const Checked chk = check_record(env, is, city, *owner, cmd.placements[i], nullptr);
        if (chk.skip) {
            continue;
        }
        if (chk.status == Reject::None) {
            result.records[i] = place(env, is, city, *owner, chk);
        } else {
            result.records[i].status = chk.status;
        }
        if (result.records[i].status == Reject::None) {
            placed_any = true;
        } else if (first_failure == Reject::None) {
            first_failure = result.records[i].status;
        }
    }
    // The command is rejected only when it changed nothing: no record placed and at
    // least one refused. A partial drag build is accepted (its records tell which).
    if (!placed_any) {
        result.status = first_failure;
    }
    return result;
}

// ---- Demolish 0x68 (§8.4) ----------------------------------------------------------

/// Removes a whole city (D9: D 0x10058f80 after city slot +0x78 reports empty; O-48:
/// only the list destructor 0x10023610 is read, so the rebuild drops the whole index):
/// every building through remove_building (derived indexes, footprint objects, the
/// market list's defeat flag), every tile of the city becomes neutral (a stand-in that
/// keeps §9.6 "every owner is a present city or neutral"; what the original does with
/// the tiles is unread, OQ-126), and the slot is emptied.
void remove_city(Env& env, Island& is, CityNo no) {
    City& city = *is.cities[no.v];
    for (std::size_t l = 0; l < city.index.lists.size(); ++l) {
        const std::size_t n = city.index.lists[l].slots.size();
        for (std::size_t s = 0; s < n; ++s) {
            if (city.index.lists[l].slots[s].has_value()) {
                static_cast<void>(
                    remove_building(env.cfg, env.types, env.players, is,
                                    BuildingKey{CityRef{is.index, no}, static_cast<ListId>(l),
                                                static_cast<Slot>(s)}));
            }
        }
    }
    for (std::uint64_t idx = 0; idx < is.tiles.tile_count(); ++idx) {
        if (is.tiles.owner(idx) == no.v) {
            is.tiles.set_owner(idx, env.cfg.neutral_owner);
        }
    }
    is.cities[no.v].reset();
}

ApplyResult demolish(Env& env, const Command& c, const DemolishCmd& cmd) {
    ApplyResult result;
    const Target t = resolve_city(env, c, cmd.city);
    if (t.status != Reject::None) {
        result.status = t.status;
        return result;
    }
    Island& is = *t.island;
    const CityNo no = cmd.city.city;
    result.records.resize(cmd.tiles.size());
    // Tile by tile in stream order; a tile that is not the city's is skipped silently
    // (D: op 6 of 0x10058e90). The command as a whole is never refused after the city
    // checks; each record tells what happened to its tile.
    for (std::size_t i = 0; i < cmd.tiles.size(); ++i) {
        RecordResult& r = result.records[i];
        City* city = is.city(no);
        if (city == nullptr) {
            r.status = Reject::UnknownCity; // the city was removed by an earlier tile (D9)
            continue;
        }
        const TileXY tile = cmd.tiles[i];
        const Slot idx = is.tile_index(tile.x, tile.y);
        if (idx == kNoSlot) {
            r.status = Reject::OutOfIsland;
        } else if (is.tiles.owner(idx) != no.v) {
            r.status = Reject::TileNotOwned;
        } else if (const Building* found = is.building_at(env.cfg.layout, no, tile.x, tile.y)) {
            // FindByTile over the lookup order (D 0x10023c80), then RemoveAt
            // (0x10028fd0): the Remove seam first (O-5), then the removal (§7.3).
            const BuildingId id = found->id;
            const auto key = is.cache.by_id.find(id);
            Building* b = find_building_mut(is, id);
            if (key == is.cache.by_id.end() || b == nullptr) {
                r.status = Reject::NoBuildingAtTile; // a derived index out of date
            } else {
                const BuildingKey k = key->second;
                run_building_seam(env, env.seams.on_removed, is, *city, *b);
                r.status = remove_building(env.cfg, env.types, env.players, is, k)
                               ? Reject::None
                               : Reject::NoBuildingAtTile;
                r.building = id;
            }
        } else if (!tile_has_building(is, idx) && is.tiles.object(idx) != 0) {
            // deviation: the non-building command path is still open (OQ-201).
            // This branch uses the building clear arm (0x10082c30) as a stand-in.
            // The named tile may be any tile of the object. A building of another
            // city on this city's tile is left alone (the branch above).
            clear_object_record(is.tiles, env.types, tile.x, tile.y);
        } else {
            r.status = Reject::NoBuildingAtTile;
        }
        // D9 (D 0x10058f80, city slot +0x78 = 0x1002f7f0, O-6): the predicate is
        // unread, so by default (no seam) no city is ever removed.
        City* still = is.city(no);
        if (still != nullptr && !env.rules.keep_empty_city &&
            env.seams.city_is_empty.fn != nullptr &&
            env.seams.city_is_empty.fn(env.seams.city_is_empty.ctx, *still)) {
            remove_city(env, is, no);
        }
    }
    return result;
}

// ---- SetTradeFlag 0x7c, SetTradeSlot 0x73 / 0x8a (§8.4) ----------------------------

/// The "none" value of a trade-list index on the wire (the original's byte 0xff, -1):
/// a wire value, not a count.
enum : std::uint32_t { kNoListIndex = 0xffffffffU };

ApplyResult set_trade_flag(Env& env, const Command& c, const SetTradeFlagCmd& cmd) {
    ApplyResult result;
    const Target t = resolve_city(env, c, cmd.city);
    if (t.status != Reject::None) {
        result.status = t.status;
        return result;
    }
    City& city = *t.city;
    // D 0x1002dbd0.
    if (cmd.goods >= city.goods.size()) {
        result.status = Reject::BadIndex;
        return result;
    }
    if (cmd.mode == 0 || cmd.mode == 1) {
        // Modes 0 / 1: the trade-list index of the goods on list `mode` (the original
        // stores the low byte of `value` at +0x0b and `mode` in bit 0 of +0x0a). The
        // rebuild takes -1 or the index of the entry of that list that holds the goods
        // (§9.6, validate_trade); anything else would be a state the loader refuses.
        std::int32_t index = -1;
        if (cmd.value != kNoListIndex) {
            const std::uint32_t list = cmd.mode;
            if (list >= city.trade_lists.size() || cmd.value >= city.trade_lists[list].size() ||
                cmd.value > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
                city.trade_lists[list][cmd.value].goods != cmd.goods) {
                result.status = Reject::BadValue;
                return result;
            }
            index = static_cast<std::int32_t>(cmd.value);
        }
        city.goods[cmd.goods].list_index = index;
        city.goods[cmd.goods].buy = cmd.mode == 1;
    } else if (cmd.mode == 9) {
        // Mode 9: the setting of a build goods (D: [city + 0x48c + 8g]); goods outside
        // the build goods are ignored.
        if (cmd.goods >= env.cfg.build_goods_first &&
            cmd.goods - env.cfg.build_goods_first < city.build_goods.size()) {
            city.build_goods[cmd.goods - env.cfg.build_goods_first].setting = cmd.value;
        }
    }
    // Any other mode is ignored (D 0x1002dbd0 handles 0, 1 and 9 only).
    return result;
}

ApplyResult set_trade_slot(Env& env, const Command& c, const SetTradeSlotCmd& cmd) {
    ApplyResult result;
    const Target t = resolve_city(env, c, cmd.city);
    if (t.status != Reject::None) {
        result.status = t.status;
        return result;
    }
    City& city = *t.city;
    // I (O-14, OQ-127): list < trade_lists, index < the list's length or kAppendIndex
    // (0x8a).
    if (cmd.list >= city.trade_lists.size()) {
        result.status = Reject::BadIndex;
        return result;
    }
    std::vector<TradeEntry>& list = city.trade_lists[cmd.list];
    if (cmd.index != kAppendIndex && cmd.index >= list.size()) {
        result.status = Reject::BadIndex;
        return result;
    }
    // An entry names a warehouse goods (§9.6); goods 0 clears an entry and appends none.
    if (cmd.goods >= env.cfg.warehouse_goods || (cmd.goods == 0 && cmd.index == kAppendIndex)) {
        result.status = Reject::BadValue;
        return result;
    }
    const TradeEntry entry =
        cmd.goods == 0 ? TradeEntry{} : TradeEntry{cmd.goods, cmd.amount_88, cmd.price};
    if (cmd.index == kAppendIndex) {
        list.push_back(entry);
        return result;
    }
    const GoodsId old = list[cmd.index].goods;
    list[cmd.index] = entry;
    // A goods setting that named this entry for the goods it held is unlinked, as the
    // original's entry removal writes 0xff into the goods byte (0x100285f4,
    // 0x10028655); so every setting keeps naming an entry that holds its goods (§9.6).
    if (old != entry.goods && old < city.goods.size()) {
        GoodsSetting& gs = city.goods[old];
        if (gs.list_index >= 0 && static_cast<std::uint32_t>(gs.list_index) == cmd.index &&
            (gs.buy ? 1U : 0U) == cmd.list) {
            gs.list_index = -1;
        }
    }
    return result;
}

} // namespace

ApplyResult Commands::apply(World& w, const Command& command, const InAdvance* in_advance) {
    Env env{w,          w.config_,  w.rules_, w.settings_,     w.seams_,  *w.types_,
            w.players_, w.islands_, w.rng_,   w.clock_.tick(), in_advance};
    ApplyResult result;
    if (w.players_.find(command.issuer) == nullptr) {
        result.status = Reject::UnknownIssuer;
    } else if (const auto* speed = std::get_if<SetSpeedCmd>(&command.body)) {
        result.status = set_speed(w.config_, w.settings_, command, *speed);
    } else if (const auto* line = std::get_if<SetProductionLineCmd>(&command.body)) {
        result.status = set_production_line(w, w.config_, *w.types_, command, *line);
    } else if (const auto* b = std::get_if<BuildCmd>(&command.body)) {
        result = build(env, command, *b);
    } else if (const auto* d = std::get_if<DemolishCmd>(&command.body)) {
        result = demolish(env, command, *d);
    } else if (const auto* f = std::get_if<SetTradeFlagCmd>(&command.body)) {
        result = set_trade_flag(env, command, *f);
    } else if (const auto* s = std::get_if<SetTradeSlotCmd>(&command.body)) {
        result = set_trade_slot(env, command, *s);
    } else {
        // A legacy record is never executed (fail closed, §8.3).
        result.status = Reject::Unsupported;
    }
    if (!result.ok()) {
        SimEvent e;
        e.kind = SimEventKind::CommandRejected;
        e.tick = w.clock_.tick();
        e.player = command.issuer;
        e.code = static_cast<std::uint32_t>(result.status);
        w.host_events_.push(e);
        if (in_advance != nullptr) {
            w.out_.events.push_back(e);
        }
    }
    return result;
}

ApplyResult World::apply(const Command& command) {
    return Commands::apply(*this, command, nullptr);
}

} // namespace anno::sim
