#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include <anno/sim/building.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

std::uint64_t terrain_digest(const Terrain& t) {
    StateHasher h;
    h.u64(t.height.size());
    for (const std::uint8_t v : t.height) {
        h.u8(v);
    }
    h.u64(t.gfx.size());
    for (const std::uint16_t v : t.gfx) {
        h.u16(v);
    }
    h.u64(t.fertility.size());
    for (const std::uint8_t v : t.fertility) {
        h.u8(v);
    }
    return nonzero_digest(h.value());
}

std::shared_ptr<const Terrain> make_terrain(std::vector<std::uint8_t> height,
                                            std::vector<std::uint16_t> gfx,
                                            std::vector<std::uint8_t> fertility) {
    auto t = std::make_shared<Terrain>();
    t->height = std::move(height);
    t->gfx = std::move(gfx);
    t->fertility = std::move(fertility);
    t->digest = terrain_digest(*t);
    return t;
}

TileLayer::TileLayer(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
                     std::uint32_t chunk_edge) {
    static_cast<void>(reset(x0, y0, w, h, chunk_edge));
}

bool TileLayer::reset(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h,
                      std::uint32_t chunk_edge) {
    if (chunk_edge == 0) {
        return false;
    }
    if (!island_tiles_within_bound(w, h)) {
        return false;
    }
    const std::uint64_t tiles = std::uint64_t{w} * h;
    x0_ = x0;
    y0_ = y0;
    w_ = w;
    h_ = h;
    edge_ = chunk_edge;
    // ceil(w / edge) in u64: w + edge - 1 may exceed u32 (A4).
    cx_ = static_cast<std::uint32_t>((std::uint64_t{w} + chunk_edge - 1) / chunk_edge);
    cy_ = static_cast<std::uint32_t>((std::uint64_t{h} + chunk_edge - 1) / chunk_edge);
    const auto n = static_cast<std::size_t>(tiles);
    object_.assign(n, 0);
    phase_.assign(n, 0);
    owner_.assign(n, 0);
    aux_.assign(n, 0);
    const auto chunks = static_cast<std::size_t>(chunk_count());
    digest_.assign(chunks, 0);
    dirty_.assign(chunks, 1);
    ignored_writes_ = 0;
    return true;
}

bool TileLayer::contains(std::int32_t x, std::int32_t y) const {
    const std::int64_t dx = std::int64_t{x} - x0_;
    const std::int64_t dy = std::int64_t{y} - y0_;
    return dx >= 0 && dy >= 0 && dx < std::int64_t{w_} && dy < std::int64_t{h_};
}

Slot TileLayer::index_of(std::int32_t x, std::int32_t y) const {
    if (!contains(x, y)) {
        return kNoSlot;
    }
    const std::int64_t dx = std::int64_t{x} - x0_;
    const std::int64_t dy = std::int64_t{y} - y0_;
    return static_cast<Slot>(dy * std::int64_t{w_} + dx);
}

std::uint32_t TileLayer::object(std::uint64_t idx) const {
    return idx < object_.size() ? object_[static_cast<std::size_t>(idx)] : 0;
}
std::uint8_t TileLayer::phase(std::uint64_t idx) const {
    return idx < phase_.size() ? phase_[static_cast<std::size_t>(idx)] : std::uint8_t{0};
}
std::uint32_t TileLayer::owner(std::uint64_t idx) const {
    return idx < owner_.size() ? owner_[static_cast<std::size_t>(idx)] : 0;
}
std::uint32_t TileLayer::aux(std::uint64_t idx) const {
    return idx < aux_.size() ? aux_[static_cast<std::size_t>(idx)] : 0;
}

std::uint64_t TileLayer::chunk_of(std::uint64_t idx) const {
    // Total: an empty layer (w_ == 0, edge_ == 0) has no valid index, and the
    // divisions below must never see a zero divisor.
    if (idx >= tile_count()) {
        return chunk_count();
    }
    const std::uint64_t y = idx / w_;
    const std::uint64_t x = idx % w_;
    return (y / edge_) * cx_ + x / edge_;
}

void TileLayer::mark(std::uint64_t idx) {
    dirty_[static_cast<std::size_t>(chunk_of(idx))] = 1;
}

void TileLayer::set_object(std::uint64_t idx, std::uint32_t v) {
    if (idx >= object_.size()) {
        ++ignored_writes_;
        return;
    }
    object_[static_cast<std::size_t>(idx)] = v;
    mark(idx);
}
void TileLayer::set_phase(std::uint64_t idx, std::uint8_t v) {
    if (idx >= phase_.size()) {
        ++ignored_writes_;
        return;
    }
    phase_[static_cast<std::size_t>(idx)] = v;
    mark(idx);
}
void TileLayer::set_owner(std::uint64_t idx, std::uint32_t v) {
    if (idx >= owner_.size()) {
        ++ignored_writes_;
        return;
    }
    owner_[static_cast<std::size_t>(idx)] = v;
    mark(idx);
}
void TileLayer::set_aux(std::uint64_t idx, std::uint32_t v) {
    if (idx >= aux_.size()) {
        ++ignored_writes_;
        return;
    }
    aux_[static_cast<std::size_t>(idx)] = v;
    mark(idx);
}

std::uint64_t TileLayer::chunk_digest_uncached(std::uint64_t chunk) const {
    if (chunk >= chunk_count()) {
        return 0;
    }
    const std::uint64_t kx = chunk % cx_;
    const std::uint64_t ky = chunk / cx_;
    const std::uint64_t x_begin = kx * edge_;
    const std::uint64_t y_begin = ky * edge_;
    // Edge chunks are clipped to the in-bounds tiles (A14, F20).
    const std::uint64_t x_end = std::min<std::uint64_t>(w_, x_begin + edge_);
    const std::uint64_t y_end = std::min<std::uint64_t>(h_, y_begin + edge_);
    StateHasher h;
    for (std::uint64_t y = y_begin; y < y_end; ++y) {
        for (std::uint64_t x = x_begin; x < x_end; ++x) {
            const auto i = static_cast<std::size_t>(y * w_ + x);
            h.u32(object_[i]);
            h.u8(phase_[i]);
            h.u32(owner_[i]);
            h.u32(aux_[i]);
        }
    }
    return h.value();
}

std::uint64_t TileLayer::chunk_digest(std::uint64_t chunk) const {
    if (chunk >= chunk_count()) {
        return 0;
    }
    const auto k = static_cast<std::size_t>(chunk);
    // Never stores: a const layer may be hashed by several threads at once.
    return dirty_[k] != 0 ? chunk_digest_uncached(chunk) : digest_[k];
}

std::uint64_t TileLayer::chunk_digest(std::uint64_t chunk) {
    if (chunk >= chunk_count()) {
        return 0;
    }
    const auto k = static_cast<std::size_t>(chunk);
    if (dirty_[k] != 0) {
        digest_[k] = chunk_digest_uncached(chunk);
        dirty_[k] = 0;
    }
    return digest_[k];
}

void TileLayer::refresh(const Executor& ex) {
    parallel_for(ex, chunk_count(), [this](std::uint64_t chunk) {
        const auto k = static_cast<std::size_t>(chunk);
        if (dirty_[k] != 0) {
            digest_[k] = chunk_digest_uncached(chunk);
            dirty_[k] = 0;
        }
    });
}

std::uint64_t TileLayer::dirty_chunks() const {
    std::uint64_t n = 0;
    for (const std::uint8_t d : dirty_) {
        n += d != 0 ? 1U : 0U;
    }
    return n;
}

bool TileLayer::cache_consistent() const {
    if (digest_.size() != chunk_count() || dirty_.size() != chunk_count()) {
        return false;
    }
    for (std::uint64_t k = 0; k < chunk_count(); ++k) {
        const auto i = static_cast<std::size_t>(k);
        if (dirty_[i] == 0 && digest_[i] != chunk_digest_uncached(k)) {
            return false;
        }
    }
    return true;
}

namespace {

std::string path_of(std::uint64_t slot) {
    return "islands[" + std::to_string(slot) + "]";
}

/// Whether two rectangles share a tile (edges in i64: x0 + w may exceed i32).
bool overlaps(const SlotRect& a, const SlotRect& b) {
    const std::int64_t ax0 = a.x0, ay0 = a.y0;
    const std::int64_t bx0 = b.x0, by0 = b.y0;
    const std::int64_t ax1 = ax0 + a.w, ay1 = ay0 + a.h;
    const std::int64_t bx1 = bx0 + b.w, by1 = by0 + b.h;
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

/// Whether a rectangle lies inside the world (edges in i64).
bool inside_world(const SlotRect& r, const WorldSettings& settings) {
    const std::int64_t x1 = std::int64_t{r.x0} + r.w;
    const std::int64_t y1 = std::int64_t{r.y0} + r.h;
    return r.x0 >= 0 && r.y0 >= 0 && x1 <= std::int64_t{settings.world_width} &&
           y1 <= std::int64_t{settings.world_height};
}

std::string outside_message(std::uint64_t slot, const WorldSettings& settings) {
    return path_of(slot) + ".tiles: the rectangle is not inside the world (" +
           std::to_string(settings.world_width) + " x " + std::to_string(settings.world_height) +
           ")";
}

/// The indexes of a city's trade data that the Kontor commands (S10) and the trade
/// consumers will use (docs/sim/state.md §9.6): every trade-list entry names a goods
/// below SimConfig::warehouse_goods, and a goods setting whose list_index is not -1
/// names an entry of the list its bit selects (D: the getter 0x1002db53 compares
/// bit 0 with the requested list) that holds this goods (D: the STADT2 loader
/// stores the loop index of the entry in the byte, 0x1002e997, 0x1002ea7c). The
/// original stores any byte a setter hands it (0x1002dbd0), so a mismatch can exist
/// there; the rebuild's command layer refuses such a value (O-14). Not checked:
/// BuildGoods::setting and CityStock::capacity (no index, range open, O-57).
/// Returns "" or the path below the city ("." + ...) and the reason.
std::string validate_trade(const City& city, const SimConfig& cfg) {
    for (std::size_t l = 0; l < city.trade_lists.size(); ++l) {
        const std::vector<TradeEntry>& list = city.trade_lists[l];
        for (std::size_t e = 0; e < list.size(); ++e) {
            if (list[e].goods >= cfg.warehouse_goods) {
                return ".trade_lists[" + std::to_string(l) + "][" + std::to_string(e) +
                       "].goods: " + std::to_string(list[e].goods) +
                       " is not below SimConfig.warehouse_goods " +
                       std::to_string(cfg.warehouse_goods);
            }
        }
    }
    for (std::size_t g = 0; g < city.goods.size(); ++g) {
        const GoodsSetting& gs = city.goods[g];
        if (gs.list_index == -1) {
            continue;
        }
        const std::string where = ".goods[" + std::to_string(g) + "].list_index: ";
        if (gs.list_index < 0) {
            return where + std::to_string(gs.list_index) + " is neither -1 (none) nor an index";
        }
        const std::size_t list = gs.buy ? 1 : 0;
        const auto index = static_cast<std::size_t>(gs.list_index);
        if (list >= city.trade_lists.size() || index >= city.trade_lists[list].size()) {
            return where + std::to_string(gs.list_index) + " names no entry of trade list " +
                   std::to_string(list);
        }
        if (city.trade_lists[list][index].goods != g) {
            return where + std::to_string(gs.list_index) + " names the entry of trade list " +
                   std::to_string(list) + " that holds goods " +
                   std::to_string(city.trade_lists[list][index].goods) + ", not " +
                   std::to_string(g);
        }
    }
    return {};
}

/// The structural facts of the cities of one island and of their buildings
/// (docs/sim/state.md §9.6): the facts the scheduler and the step functions rely
/// on and that need nothing but the configuration and the registry. The facts
/// that need the player table (a city's owner exists) or the derived indexes
/// (footprints, id map, counters) are World::check_invariants'. Empty when they
/// hold.
std::string validate_cities(const Island& is, std::size_t slot, const SimConfig& cfg,
                            const TypeRegistry& types) {
    const std::string p = path_of(slot);
    if (is.cities.size() != cfg.city_slots) {
        return p + ".cities: " + std::to_string(is.cities.size()) +
               " slots, SimConfig.city_slots is " + std::to_string(cfg.city_slots);
    }
    for (std::size_t c = 0; c < is.cities.size(); ++c) {
        if (!is.cities[c].has_value()) {
            continue;
        }
        const City& city = *is.cities[c];
        const std::string cp = p + ".cities[" + std::to_string(c) + "]";
        if (city.number.v != c) {
            return cp + ".number: is " + std::to_string(city.number.v) + ", the slot is " +
                   std::to_string(c);
        }
        if (city.stock.stock.size() != cfg.warehouse_goods) {
            return cp + ".stock: " + std::to_string(city.stock.stock.size()) +
                   " goods, SimConfig.warehouse_goods is " + std::to_string(cfg.warehouse_goods);
        }
        if (city.goods.size() != cfg.warehouse_goods) {
            return cp + ".goods: " + std::to_string(city.goods.size()) +
                   " settings, SimConfig.warehouse_goods is " + std::to_string(cfg.warehouse_goods);
        }
        if (city.build_goods.size() != cfg.build_goods_count) {
            return cp + ".build_goods: " + std::to_string(city.build_goods.size()) +
                   " records, SimConfig.build_goods_count is " +
                   std::to_string(cfg.build_goods_count);
        }
        if (city.trade_lists.size() != cfg.trade_lists) {
            return cp + ".trade_lists: " + std::to_string(city.trade_lists.size()) +
                   " lists, SimConfig.trade_lists is " + std::to_string(cfg.trade_lists);
        }
        {
            const std::string problem = validate_trade(city, cfg);
            if (!problem.empty()) {
                return cp + problem;
            }
        }
        if (city.index.lists.size() != cfg.layout.lists.size()) {
            return cp + ".index.lists: " + std::to_string(city.index.lists.size()) +
                   " lists, SimConfig.layout has " + std::to_string(cfg.layout.lists.size());
        }
        for (std::size_t l = 0; l < city.index.lists.size(); ++l) {
            const BuildingList& list = city.index.lists[l];
            const std::string lp = cp + ".index.lists[" + std::to_string(l) + "]";
            const std::uint64_t n = list.slots.size();
            if (n < 1) {
                return lp + ".slots: the list has no element (a fresh list holds one null, A7)";
            }
            if (list.hint < -1 || list.hint >= static_cast<std::int64_t>(n)) {
                return lp + ".hint: " + std::to_string(list.hint) + " is outside -1 .. " +
                       std::to_string(n);
            }
            if (list.cursor >= n) {
                return lp + ".cursor: " + std::to_string(list.cursor) + " is not below the " +
                       std::to_string(n) + " slots (A8)";
            }
            for (std::size_t s = 0; s < list.slots.size(); ++s) {
                if (!list.slots[s].has_value()) {
                    continue;
                }
                const Building& b = *list.slots[s];
                const std::string bp = lp + ".slots[" + std::to_string(s) + "]";
                const BuildingType* ty = types.find(b.type);
                if (ty == nullptr) {
                    return bp + ".type: " + std::to_string(b.type.v) + " is not in the registry";
                }
                if (b.rotation > 3) {
                    return bp + ".rotation: " + std::to_string(b.rotation) + " is not below 4";
                }
                const std::optional<ListId> home = cfg.layout.list_for(ty->prodtype);
                if (!home.has_value() || *home != l) {
                    return bp + ": the building sits in list " + std::to_string(l) +
                           " but its type belongs to " +
                           (home.has_value() ? "list " + std::to_string(*home) : "no list");
                }
                if (island_of(b.id) != is.index) {
                    return bp + ".id: names island " + std::to_string(island_of(b.id).v) +
                           ", the island is " + std::to_string(is.index.v);
                }
                if (serial_of(b.id) == 0 || serial_of(b.id) >= is.next_serial) {
                    return bp + ".id: serial " + std::to_string(serial_of(b.id)) +
                           " is not in 1 .. next_serial (" + std::to_string(is.next_serial) + ")";
                }
                const std::string problem =
                    payload_problem(b.payload, *ty, types.class_info(ty->cls), cfg);
                if (!problem.empty()) {
                    return bp + ".payload: " + problem;
                }
                if (const auto* mine = std::get_if<Mine>(&b.payload)) {
                    if (mine->deposit.has_value() && *mine->deposit >= is.deposits.size()) {
                        return bp + ".payload.deposit: index " + std::to_string(*mine->deposit) +
                               " is outside the " + std::to_string(is.deposits.size()) +
                               " deposits";
                    }
                }
            }
        }
    }
    return {};
}

/// The facts of one island (everything but the disjointness and the total area,
/// which need the whole table: validate_island_rects). Empty when they hold.
std::string validate_island(const Island& is, std::size_t slot, const SimConfig& cfg,
                            const WorldSettings& settings, const TypeRegistry& types) {
    const std::string p = path_of(slot);
    if (is.index.v != slot) {
        return p + ".index: is " + std::to_string(is.index.v) + ", the slot is " +
               std::to_string(slot);
    }
    const TileLayer& t = is.tiles;
    if (t.width() == 0 || t.height() == 0) {
        return p + ".tiles: the island rectangle is empty";
    }
    if (t.chunk_edge() != cfg.chunk_edge) {
        return p + ".tiles: chunk_edge is " + std::to_string(t.chunk_edge()) +
               ", SimConfig.chunk_edge is " + std::to_string(cfg.chunk_edge);
    }
    if (!island_tiles_within_bound(t.width(), t.height())) {
        return p + ".tiles: more than " + std::to_string(kIslandTileBound) + " tiles";
    }
    const std::int64_t x1 = std::int64_t{t.x0()} + t.width();
    const std::int64_t y1 = std::int64_t{t.y0()} + t.height();
    if (!inside_world(SlotRect{slot, t.x0(), t.y0(), t.width(), t.height()}, settings)) {
        return outside_message(slot, settings);
    }
    if (is.climate >= cfg.monopoly_climate_mask.size()) {
        return p + ".climate: " + std::to_string(is.climate) + " has no entry in " +
               "SimConfig.monopoly_climate_mask";
    }
    if (is.next_serial < 1) {
        return p + ".next_serial: must be at least 1";
    }
    {
        const std::string problem = validate_cities(is, slot, cfg, types);
        if (!problem.empty()) {
            return problem;
        }
    }
    for (std::uint64_t i = 0; i < t.tile_count(); ++i) {
        if (t.owner(i) > cfg.neutral_owner) {
            return p + ".tiles: tile " + std::to_string(i) + " has owner " +
                   std::to_string(t.owner(i)) + ", above neutral_owner " +
                   std::to_string(cfg.neutral_owner);
        }
        // S6: an owner other than "nobody" is the number of a present city (§9.6).
        if (t.owner(i) != cfg.neutral_owner && is.city(CityNo{t.owner(i)}) == nullptr) {
            return p + ".tiles: tile " + std::to_string(i) + " has owner " +
                   std::to_string(t.owner(i)) + ", which is not a city of the island";
        }
    }
    if (is.sweep.grow_x >= t.width() || is.sweep.fauna_x >= t.width() ||
        is.sweep.grow_y >= t.height() || is.sweep.fauna_y >= t.height()) {
        return p + ".sweep: a sweep position is outside the island";
    }
    // D: the positions start at 0 (0x1008cb72..0x1008cb87) and advance by the
    // chunk edge (add edi, 8: 0x1008b7d9..0x1008b7ef, 0x1008b851..0x1008b867), so
    // no other value is ever stored.
    if (is.sweep.grow_x % cfg.chunk_edge != 0 || is.sweep.grow_y % cfg.chunk_edge != 0 ||
        is.sweep.fauna_x % cfg.chunk_edge != 0 || is.sweep.fauna_y % cfg.chunk_edge != 0) {
        return p + ".sweep: a sweep position is not a multiple of SimConfig.chunk_edge";
    }
    if (is.ring.cursor >= cfg.city_slots) {
        return p + ".ring.cursor: " + std::to_string(is.ring.cursor) + " is outside the " +
               std::to_string(cfg.city_slots) + " city slots";
    }
    if (is.objmgr.cursor >= std::max<std::size_t>(is.deposits.size(), 1)) {
        return p + ".objmgr.cursor: " + std::to_string(is.objmgr.cursor) + " is outside the " +
               std::to_string(is.deposits.size()) + " deposits";
    }
    for (std::size_t d = 0; d < is.deposits.size(); ++d) {
        const Deposit& dep = is.deposits[d];
        const std::string dp = p + ".deposits[" + std::to_string(d) + "]";
        if (dep.x < t.x0() || dep.y < t.y0() || std::int64_t{dep.x} >= x1 ||
            std::int64_t{dep.y} >= y1) {
            return dp + ": the position is outside the island";
        }
        if (!types.goods_by_name(dep.goods).has_value()) {
            return dp + ".goods: unknown goods name '" + dep.goods + "'";
        }
    }
    if (is.reserved_object_count != 0) {
        return p + ".reserved_object_count: is " + std::to_string(is.reserved_object_count) +
               ", must be 0 until the island objects are ported (O-12)";
    }
    if (is.drought.has_value()) {
        const Drought& dr = *is.drought;
        const std::string dp = p + ".drought";
        const std::uint64_t step_limit = std::uint64_t{cfg.drought_steps} * cfg.chunk_edge;
        if (dr.state != 0 && dr.state != 1 && dr.state != 0xff) {
            return dp + ".state: " + std::to_string(dr.state) + " is not 0, 1 or 255";
        }
        // The machine resets phase, step and counter in the tick that reaches the
        // limit (the original compares with jb: 0x1008b4f7, 0x1008b516,
        // 0x1008b5be), so a stored value is strictly below it.
        if (dr.phase >= cfg.drought_phases) {
            return dp + ".phase: " + std::to_string(dr.phase) +
                   " is not below SimConfig.drought_phases";
        }
        if (dr.step >= step_limit) {
            return dp + ".step: " + std::to_string(dr.step) +
                   " is not below SimConfig.drought_steps * chunk_edge";
        }
        if (dr.counter >= cfg.drought_plateau_count) {
            return dp + ".counter: " + std::to_string(dr.counter) +
                   " is not below SimConfig.drought_plateau_count";
        }
        // The coordinate space of x and y (island or world) is fixed by S13; both
        // are below the world edge, and both are floored to the chunk edge.
        if (dr.x % cfg.chunk_edge != 0 || dr.y % cfg.chunk_edge != 0) {
            return dp + ".x/y: " + std::to_string(dr.x) + ", " + std::to_string(dr.y) +
                   " are not multiples of SimConfig.chunk_edge";
        }
        if (dr.x >= settings.world_width || dr.y >= settings.world_height) {
            return dp + ".x/y: " + std::to_string(dr.x) + ", " + std::to_string(dr.y) +
                   " are outside the world";
        }
        // ring is i32 and runs 0 .. target_ring: an expanding ring past the target
        // turns into the plateau, a retreating one below 0 removes the drought, both
        // in the same tick.
        if (dr.ring < 0 || std::int64_t{dr.ring} > std::int64_t{dr.target_ring}) {
            return dp + ".ring: " + std::to_string(dr.ring) + " is not in 0 .. target_ring (" +
                   std::to_string(dr.target_ring) + ")";
        }
        if (cfg.city_slots > 32) {
            return dp + ".city_mask: a u32 mask holds 32 city slots, SimConfig.city_slots is " +
                   std::to_string(cfg.city_slots);
        }
        const std::uint32_t slot_bits =
            cfg.city_slots >= 32 ? 0xFFFFFFFFU : (std::uint32_t{1} << cfg.city_slots) - 1U;
        if ((dr.city_mask & ~slot_bits) != 0) {
            return dp + ".city_mask: bits beyond the " + std::to_string(cfg.city_slots) +
                   " city slots are set";
        }
    }
    if (!t.cache_consistent()) {
        return p + ".tiles: a cached chunk digest is out of date";
    }
    return {};
}

} // namespace

std::string validate_island_rects(const std::vector<SlotRect>& rects,
                                  const WorldSettings& settings) {
    // 1. Every rectangle inside the world. After this, no rectangle is wider
    // than the world, so the sums below stay small.
    std::uint64_t area = 0;
    for (const SlotRect& r : rects) {
        if (!inside_world(r, settings)) {
            return outside_message(r.slot, settings);
        }
        area += std::uint64_t{r.w} * r.h;
    }
    // 2. Rectangles that are inside the world cover at most its area, unless two
    // of them share a tile (an early exit before the sweep of the next step).
    const std::uint64_t world_area = std::uint64_t{settings.world_width} * settings.world_height;
    if (area > world_area) {
        return "islands: the rectangles cover " + std::to_string(area) + " tiles, more than the " +
               std::to_string(world_area) + " of the world";
    }
    // 3. Disjoint rectangles: sweep in (x0, y0, slot) order, a total order, and
    // only compare rectangles whose x ranges overlap.
    std::vector<std::size_t> order(rects.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const SlotRect& ra = rects[a];
        const SlotRect& rb = rects[b];
        if (ra.x0 != rb.x0) {
            return ra.x0 < rb.x0;
        }
        if (ra.y0 != rb.y0) {
            return ra.y0 < rb.y0;
        }
        return ra.slot < rb.slot;
    });
    for (std::size_t i = 0; i < order.size(); ++i) {
        const SlotRect& a = rects[order[i]];
        const std::int64_t a_end = std::int64_t{a.x0} + a.w;
        for (std::size_t j = i + 1; j < order.size(); ++j) {
            const SlotRect& b = rects[order[j]];
            if (std::int64_t{b.x0} >= a_end) {
                break;
            }
            if (overlaps(a, b)) {
                return path_of(std::max(a.slot, b.slot)) + ".tiles: the rectangle overlaps " +
                       path_of(std::min(a.slot, b.slot));
            }
        }
    }
    return {};
}

std::string validate_islands(const std::vector<std::optional<Island>>& islands,
                             const SimConfig& cfg, const WorldSettings& settings,
                             const TypeRegistry& types) {
    if (islands.size() > cfg.island_slots) {
        return "islands: " + std::to_string(islands.size()) + " slots, SimConfig.island_slots is " +
               std::to_string(cfg.island_slots);
    }
    std::vector<SlotRect> rects;
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (!islands[i].has_value()) {
            continue;
        }
        std::string problem = validate_island(*islands[i], i, cfg, settings, types);
        if (!problem.empty()) {
            return problem;
        }
        const TileLayer& t = islands[i]->tiles;
        rects.push_back(SlotRect{i, t.x0(), t.y0(), t.width(), t.height()});
    }
    return validate_island_rects(rects, settings);
}

// ---- queries (docs/sim/state.md §4.7, §4.8, stage S6)

const Building* Island::find_building(BuildingId id) const {
    const auto it = cache.by_id.find(id);
    if (it == cache.by_id.end()) {
        return nullptr;
    }
    const BuildingKey& key = it->second;
    const City* c = city(key.city.city);
    if (c == nullptr || key.list >= c->index.lists.size()) {
        return nullptr;
    }
    const BuildingList& list = c->index.lists[key.list];
    if (key.slot >= list.slots.size() || !list.slots[key.slot].has_value() ||
        list.slots[key.slot]->id != id) {
        return nullptr;
    }
    return &*list.slots[key.slot];
}

const Building* Island::building_at(const ListLayout& layout, CityNo no, std::int32_t x,
                                    std::int32_t y, std::uint32_t list_mask) const {
    const Slot idx = tiles.index_of(x, y);
    if (idx == kNoSlot) {
        return nullptr;
    }
    for (const ListId lid : layout.lookup_order) {
        if (lid >= layout.lists.size() || lid >= cache.grids.size() ||
            (layout.lists[lid].lookup_mask & list_mask) == 0) {
            continue;
        }
        const std::vector<std::uint32_t>& grid = cache.grids[lid];
        if (idx >= grid.size() || grid[idx] == 0) {
            continue;
        }
        const BuildingId id = make_building_id(index, grid[idx]);
        const auto it = cache.by_id.find(id);
        if (it == cache.by_id.end() || it->second.city.city != no) {
            continue;
        }
        return find_building(id);
    }
    return nullptr;
}

void Island::rect_query(const ListLayout& layout, CityNo no, const TileRect& rect,
                        std::uint32_t list_mask, std::vector<BuildingId>& out) const {
    out.clear();
    const TileBox box = clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(),
                                      rect.x0, rect.y0, rect.w, rect.h);
    if (box.empty()) {
        return;
    }
    // Allocation-free but for `out` (docs/sim/state.md §4.7): the ids of one list are
    // appended, then that segment is sorted by (y, x, id) in place and made unique;
    // the lists follow in layout.rect_order, so the segments are already in the order
    // of the first key.
    const auto before = [this](BuildingId a, BuildingId b) {
        const Building* ba = find_building(a);
        const Building* bb = find_building(b);
        if (ba != nullptr && bb != nullptr) {
            if (ba->y != bb->y) {
                return ba->y < bb->y;
            }
            if (ba->x != bb->x) {
                return ba->x < bb->x;
            }
        }
        return a < b;
    };
    for (const ListId lid : layout.rect_order) {
        if (lid >= layout.lists.size() || lid >= cache.grids.size() ||
            (layout.lists[lid].lookup_mask & list_mask) == 0 || cache.grids[lid].empty()) {
            continue;
        }
        const std::vector<std::uint32_t>& grid = cache.grids[lid];
        const auto segment = static_cast<std::ptrdiff_t>(out.size());
        for (std::int64_t gy = box.y0; gy < box.y1; ++gy) {
            for (std::int64_t gx = box.x0; gx < box.x1; ++gx) {
                const std::uint32_t serial =
                    grid[static_cast<std::size_t>(gy * tiles.width() + gx)];
                if (serial == 0) {
                    continue;
                }
                const BuildingId id = make_building_id(index, serial);
                const auto it = cache.by_id.find(id);
                if (it == cache.by_id.end() || it->second.city.city != no ||
                    find_building(id) == nullptr) {
                    continue;
                }
                out.push_back(id);
            }
        }
        // The same building is met once per footprint tile in the box.
        const auto first = out.begin() + segment;
        std::sort(first, out.end(), before);
        out.erase(std::unique(first, out.end()), out.end());
    }
}

const Building* City::find_building(const Island& island, BuildingId id) const {
    const auto it = island.cache.by_id.find(id);
    if (it == island.cache.by_id.end() || it->second.city.city != number) {
        return nullptr;
    }
    return island.find_building(id);
}

const Building* City::building_at(const Island& island, const ListLayout& layout, std::int32_t x,
                                  std::int32_t y, std::uint32_t list_mask) const {
    return island.building_at(layout, number, x, y, list_mask);
}

void City::rect_query(const Island& island, const ListLayout& layout, const TileRect& rect,
                      std::uint32_t list_mask, std::vector<BuildingId>& out) const {
    island.rect_query(layout, number, rect, list_mask, out);
}

} // namespace anno::sim
