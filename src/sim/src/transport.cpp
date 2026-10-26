// Transport: the market cart and the workshop carrier (docs/sim/state.md §5.9
// "Transport", task T24; evidence docs/re/transport.md). Integer only; no draw.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <anno/sim/driver.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_search.hpp>

namespace anno::sim {

namespace {

/// √2 · 2^24 rounded (the f32 1.41421354 of 0x100c3130 times 2^24).
constexpr std::uint64_t kDiagonalQ24 = 23726566;

/// The list masks of the candidate queries (the city's building index +0x144, slot +4 =
/// 0x10023ea0; ListLayout::lookup_mask): the cart asks 8, the production list
/// (0x1005adea), the carrier 0xa, the production and the market lists (0x1005a60a).
constexpr std::uint32_t kCartMask = 0x8;
constexpr std::uint32_t kCarrierMask = 0xa;

/// The cart's stop score (0x1005aa59) and the 1 t minimum of the filters and the
/// "other goods" branch (0x1005acc7, 0x1005aacc, 0x1007296e).
constexpr std::int64_t kStopScore = 128;
constexpr std::int64_t kOneTon = 256;

/// The goods "ALL" a market's output 0 names (0x10025470).
constexpr GoodsId kAllGoods = 0x8e;

/// Goods 1 .. 25 are the raw and intermediate goods of the 75 % rule (0x1005a9b3).
bool raw_goods(GoodsId g) {
    return g >= 1 && g <= 25;
}

std::int32_t clamp32(std::int64_t v) {
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(
        v, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}

/// Floor division by 256 for a Q24.8 coordinate (the tile of a position).
std::int32_t tile_of(std::int32_t q) {
    const std::int64_t v = q;
    return static_cast<std::int32_t>(v >= 0 ? v / 256 : -((-v + 255) / 256));
}

WalkPoint walk_point(FixedPos p) {
    const TileXY tile{tile_of(p.x), tile_of(p.y)};
    const auto subcell = [](std::int32_t q, std::int32_t t) {
        const std::int64_t within = std::int64_t{q} - std::int64_t{t} * 256;
        return static_cast<std::uint32_t>(within * 3 / 256);
    };
    return WalkPoint{tile, subcell(p.x, tile.x), subcell(p.y, tile.y)};
}

std::vector<WalkPoint> entrance_points(const Building& b, const BuildingType& type) {
    std::vector<WalkPoint> out;
    out.reserve(type.entrances.size());
    for (std::size_t i = 0; i < type.entrances.size(); ++i) {
        out.push_back(walk_point(entrance_position(b, type, i)));
    }
    return out;
}

FixedPos centre(TileXY t) {
    return FixedPos{clamp32(std::int64_t{t.x} * 256 + 128), clamp32(std::int64_t{t.y} * 256 + 128)};
}

std::uint64_t isqrt(std::uint64_t v) {
    std::uint64_t r = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;
    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

bool in_footprint(const Building& b, const BuildingType& type, std::int32_t x, std::int32_t y) {
    const auto [w, h] = footprint(type, b.rotation);
    return x >= b.x && y >= b.y && std::int64_t{x} < std::int64_t{b.x} + w &&
           std::int64_t{y} < std::int64_t{b.y} + h;
}

std::vector<TileXY> footprint_tiles(const Building& b, const BuildingType& type) {
    std::vector<TileXY> out;
    const auto [w, h] = footprint(type, b.rotation);
    for (std::uint32_t j = 0; j < h; ++j) {
        for (std::uint32_t i = 0; i < w; ++i) {
            out.push_back(TileXY{clamp32(std::int64_t{b.x} + i), clamp32(std::int64_t{b.y} + j)});
        }
    }
    return out;
}

/// The building's city (its BuildingKey), or nullopt.
std::optional<CityRef> city_of(const SerialWorld& world, BuildingId id) {
    const Island* is = world.island(island_of(id));
    if (is == nullptr) {
        return std::nullopt;
    }
    const auto it = is->cache.by_id.find(id);
    if (it == is->cache.by_id.end()) {
        return std::nullopt;
    }
    return it->second.city;
}

bool has_output_store(const Payload& p) {
    return std::holds_alternative<Workshop>(p) || std::holds_alternative<WeaponsWorkshop>(p) ||
           std::holds_alternative<Farm>(p) || std::holds_alternative<SmallFarm>(p) ||
           std::holds_alternative<Mine>(p);
}

std::uint32_t reserved_of(const Building& b, std::uint32_t line) {
    if (const auto* w = std::get_if<Workshop>(&b.payload)) {
        return line < w->reserved.size() ? w->reserved[line] : 0;
    }
    if (const auto* w = std::get_if<WeaponsWorkshop>(&b.payload)) {
        return line < w->reserved.size() ? w->reserved[line] : 0;
    }
    if (const auto* s = std::get_if<SmallFarm>(&b.payload)) {
        return line < s->reserved.size() ? s->reserved[line] : 0;
    }
    if (const auto* f = std::get_if<Farm>(&b.payload)) {
        return line == 0 ? f->reserved : 0;
    }
    if (const auto* m = std::get_if<Mine>(&b.payload)) {
        return line == 0 ? m->reserved : 0;
    }
    return 0;
}

std::uint32_t out_of(const Building& b, std::uint32_t line) {
    if (const auto* w = std::get_if<Workshop>(&b.payload)) {
        return line < w->out.size() ? w->out[line] : 0;
    }
    if (const auto* w = std::get_if<WeaponsWorkshop>(&b.payload)) {
        return line < w->out.size() ? w->out[line] : 0;
    }
    if (const auto* s = std::get_if<SmallFarm>(&b.payload)) {
        return line < s->out.size() ? s->out[line] : 0;
    }
    if (const auto* f = std::get_if<Farm>(&b.payload)) {
        return line == 0 ? f->out : 0;
    }
    if (const auto* m = std::get_if<Mine>(&b.payload)) {
        return line == 0 ? m->out : 0;
    }
    return 0;
}

GoodsId listed_goods(const std::vector<GoodsId>& v, std::size_t i) {
    return i < v.size() ? v[i] : GoodsId{0};
}

/// A source's output goods i (slot +0xa0): ALL for output 0 of a market, else the
/// type's (0x100253d0, 0x10025470).
GoodsId source_output(const Building& b, const BuildingType& t, std::size_t i) {
    if (std::holds_alternative<Market>(b.payload)) {
        return i == 0 ? kAllGoods : GoodsId{0};
    }
    return listed_goods(t.output_goods, i);
}

/// The type's output-0 capacity (type +0xc0 = +0x208).
std::int64_t output_capacity(const BuildingType& t) {
    return t.has_prod && !t.prod.capacity.empty() ? std::int64_t{t.prod.capacity[0]} : 0;
}

} // namespace

bool transport_runs_job(std::uint32_t job) {
    return job == kJobMarketCart || job == kJobWorkshopCarrier;
}

std::vector<std::uint32_t> disc_half_widths(std::uint32_t r) {
    // D 0x10002a60 (0x10002adf-0x10002b75), u32 arithmetic as the registers.
    std::vector<std::uint32_t> table(static_cast<std::size_t>(r) + 1, 0);
    std::uint32_t edi = r * r;
    std::uint32_t esi = (r - 1) * r;
    std::uint32_t d = 2 * r;
    std::uint32_t ebp = 0;
    std::int64_t i = 0;
    std::int64_t j = r;
    while (i <= j) {
        table[static_cast<std::size_t>(i)] = static_cast<std::uint32_t>(j);
        edi -= ebp + 1;
        if (edi <= esi) { // `cmp edi, esi; ja` (unsigned)
            table[static_cast<std::size_t>(j)] = static_cast<std::uint32_t>(i);
            --j;
            d -= 2;
            esi -= d;
        }
        ++i;
        ebp += 2;
    }
    if (r <= 3) {
        // 0x10002b50-0x10002b73: rows r - (r + 1) / 4 .. r widened by one.
        for (std::uint32_t k = r - (r + 1) / 4; k <= r; ++k) {
            ++table[k];
        }
    }
    return table;
}

TileRect SearchArea::square() const {
    const std::int64_t x0 = std::int64_t{cx0} - radius, y0 = std::int64_t{cy0} - radius;
    const std::int64_t x1 = std::int64_t{cx1} + radius + 1, y1 = std::int64_t{cy1} + radius + 1;
    return TileRect{clamp32(x0), clamp32(y0), static_cast<std::uint32_t>(x1 - x0),
                    static_cast<std::uint32_t>(y1 - y0)};
}

bool SearchArea::contains(std::int32_t x, std::int32_t y) const {
    const std::int64_t dy = y < cy0 ? std::int64_t{cy0} - y : (y > cy1 ? std::int64_t{y} - cy1 : 0);
    const std::int64_t dx = x < cx0 ? std::int64_t{cx0} - x : (x > cx1 ? std::int64_t{x} - cx1 : 0);
    return dy <= radius && static_cast<std::size_t>(dy) < half.size() &&
           dx <= std::int64_t{half[static_cast<std::size_t>(dy)]};
}

SearchArea search_area(const Building& b, const BuildingType& type) {
    // D 0x1005ad29-0x1005adbe and 0x10002a60: centre x + (w - 1) / 2, and a second
    // centre column (w - 1) mod 2 to the right (an even footprint).
    const auto [w, h] = footprint(type, b.rotation);
    SearchArea a;
    a.cx0 = clamp32(std::int64_t{b.x} + (std::int64_t{w} - 1) / 2);
    a.cx1 = clamp32(std::int64_t{a.cx0} + (std::int64_t{w} - 1) % 2);
    a.cy0 = clamp32(std::int64_t{b.y} + (std::int64_t{h} - 1) / 2);
    a.cy1 = clamp32(std::int64_t{a.cy0} + (std::int64_t{h} - 1) % 2);
    a.radius = type.action_radius;
    a.half = disc_half_widths(a.radius);
    return a;
}

std::uint32_t store_line(const Building& b, const BuildingType& type, GoodsId g) {
    if (std::holds_alternative<SmallFarm>(b.payload)) {
        // 0x1006f9a0: line 1 when g is output goods 1.
        return type.output_goods.size() > 1 && g == type.output_goods[1] ? 1U : 0U;
    }
    if (std::holds_alternative<WeaponsWorkshop>(b.payload)) {
        // 0x1006a4a0: line 1 when g is not output goods 0.
        return g != listed_goods(type.output_goods, 0) ? 1U : 0U;
    }
    return 0; // 0x10068880: one store, g not read
}

std::int64_t available(const Building& b, const BuildingType& type, GoodsId g, const City* city) {
    if (std::holds_alternative<Market>(b.payload)) {
        // 0x10025490: the city's stock of g (city +0x7c).
        return city != nullptr && g < city->stock.stock.size() ? std::int64_t{city->stock.stock[g]}
                                                               : 0;
    }
    if (!has_output_store(b.payload)) {
        return 0;
    }
    const std::uint32_t line = store_line(b, type, g);
    return std::int64_t{out_of(b, line)} - std::int64_t{reserved_of(b, line)};
}

std::int32_t workshop_priority(const SerialWorld& world, const Building& workshop,
                               const BuildingType& type, GoodsId g) {
    // D 0x10068500: the index of g among the inputs (type +0xb0(i), i < +0xdc(1)).
    std::optional<std::size_t> index;
    for (std::size_t i = 0; i < type.input_goods.size(); ++i) {
        if (type.input_goods[i] == g) {
            index = i;
            break;
        }
    }
    const auto* w = std::get_if<Workshop>(&workshop.payload);
    if (!index || w == nullptr) {
        return -1;
    }
    // +0xbc(g) = 0x100683c0: input 1 when g is the type's input goods 1, else input 0.
    const std::size_t store = type.input_goods.size() > 1 && g == type.input_goods[1] ? 1 : 0;
    std::int64_t sum = store < w->in.size() ? std::int64_t{w->in[store]} : 0;
    // The carriers of the workshop (its slots +0x28 / +0x2c: the figures that still hold
    // its counter) whose cargo is g: reserved (cargo +0x18) and carried (+8).
    for (const auto& slot : world.figures().list.slots) {
        if (slot && slot->home == workshop.id && slot->home_counter == 0 &&
            !slot->expiry_stamp.has_value() && slot->cargo_goods == g) {
            sum += std::int64_t{slot->transport.reserved_88} + slot->cargo_88;
        }
    }
    // / type +0xe4(i) (the input per step, +0x202 + 2i); a need of 0 would divide by
    // zero in the original (no shipped workshop has one): -1 here.
    const std::int64_t need =
        *index < type.prod.input_per_step.size() ? type.prod.input_per_step[*index] : 0;
    if (need == 0) {
        return -1;
    }
    const std::int64_t steps = sum / need;
    if (steps < 30) {
        return 0x80;
    }
    if (steps < 60) {
        return 0x60;
    }
    return -1;
}

std::int64_t workshop_offer(const SerialWorld& world, const TypeRegistry& types,
                            const Building& workshop, const BuildingType& type,
                            const Building& source, const City* source_city, GoodsId& goods) {
    // D 0x100728f0 (workshop slot +0xd4).
    const BuildingType* st = types.find(source.type);
    if (st == nullptr) {
        return -1;
    }
    std::int64_t best = -1;
    auto consider = [&](GoodsId g) {
        const std::int64_t avail = available(source, *st, g, source_city);
        if (avail < kOneTon) {
            return;
        }
        const std::int32_t p = workshop_priority(world, workshop, type, g);
        if (p <= 0) {
            return;
        }
        const std::int64_t cap = output_capacity(*st);
        if (cap == 0) {
            return; // idiv by 0 in the original (P: no source type has a 0 capacity)
        }
        const std::int64_t v = p + avail * 192 / cap;
        if (v > best) {
            best = v;
            goods = g;
        }
    };
    for (std::size_t i = 0; i < type.input_goods.size(); ++i) {
        const GoodsId g = type.input_goods[i];
        const GoodsId s0 = source_output(source, *st, 0);
        if (s0 == kAllGoods || s0 == g) {
            consider(g);
        }
        if (s0 != kAllGoods && source_output(source, *st, 1) == g) {
            consider(g);
        }
    }
    return best;
}

std::uint64_t segment_length_q24(FixedPos a, FixedPos b) {
    const std::uint64_t dx = static_cast<std::uint64_t>(std::abs(std::int64_t{b.x} - a.x));
    const std::uint64_t dy = static_cast<std::uint64_t>(std::abs(std::int64_t{b.y} - a.y));
    if (dx == 0 || dy == 0) {
        return (dx + dy) << 16; // 1/256 tile -> 2^-24 tile
    }
    if (dx == dy) {
        return dx * kDiagonalQ24 / 256;
    }
    return isqrt(dx * dx + dy * dy) << 16;
}

std::optional<SourceChoice> choose_source(const SerialWorld& world, const TypeRegistry& types,
                                          std::uint32_t job, const Building& home,
                                          const BuildingType& home_type, CityRef city,
                                          std::uint32_t capacity_88, std::uint32_t layer,
                                          TransportStats* stats) {
    const Island* island = world.island(city.island);
    const City* c = world.city(city);
    if (island == nullptr || c == nullptr) {
        return std::nullopt;
    }
    const bool cart = job == kJobMarketCart;
    const SearchArea area = search_area(home, home_type);
    // Candidates: the home city's buildings of the masked lists in the square, by the
    // filter (cart 0x1005aca0: 1 t of output 0 or 1; carrier 0x1005a500: the workshop's
    // offer above 0).
    struct Candidate {
        const Building* b;
        const BuildingType* t;
        bool scored = false;
    };
    std::vector<Candidate> candidates;
    std::vector<BuildingId> ids;
    world.rect_query(city, area.square(), cart ? kCartMask : kCarrierMask, ids);
    for (const BuildingId id : ids) {
        const Building* b = world.find_building(id);
        const BuildingType* t = b != nullptr ? types.find(b->type) : nullptr;
        if (t == nullptr || b->id == home.id) {
            continue;
        }
        if (cart) {
            if (available(*b, *t, listed_goods(t->output_goods, 0), c) >= kOneTon ||
                available(*b, *t, listed_goods(t->output_goods, 1), c) >= kOneTon) {
                candidates.push_back(Candidate{b, t});
            }
        } else {
            GoodsId g = 0;
            if (workshop_offer(world, types, home, home_type, *b, c, g) > 0) {
                candidates.push_back(Candidate{b, t});
            }
        }
    }
    if (candidates.empty()) {
        if (stats != nullptr) {
            ++stats->no_candidate;
        }
        return std::nullopt;
    }
    // T29 diagnostic (home_no_road, goal_no_road): "no road among the footprint's
    // 4-neighbours" per home and per candidate. Counted on every call that reaches the
    // candidate stage (not only on `unreached`): a sender with no road at all cannot
    // reach any source, even on the open-land rule, since the test is per the owner's
    // rule in OQ-103 that a sender needs road to a market. Walk-class 1 is "road"
    // (types.cpp: `flags_word & kRoadFlag -> 1`, task T27).
    if (stats != nullptr) {
        constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kOrtho{
            {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
        const auto has_road_4nei = [&](const Building& bb, const BuildingType& tt) {
            for (const TileXY& t : footprint_tiles(bb, tt)) {
                for (const auto& [dx, dy] : kOrtho) {
                    const std::int32_t nx = t.x + dx, ny = t.y + dy;
                    if (in_footprint(bb, tt, nx, ny)) {
                        continue;
                    }
                    if (!island->tiles.contains(nx, ny)) {
                        continue;
                    }
                    const Slot idx = island->tile_index(nx, ny);
                    if (idx == kNoSlot) {
                        continue;
                    }
                    const std::uint32_t obj = island->tiles.object(idx);
                    if (obj == 0) {
                        continue;
                    }
                    const BuildingType* tt_obj = types.find(TypeId{obj});
                    if (tt_obj != nullptr && tt_obj->walk_class == 1) {
                        return true;
                    }
                }
            }
            return false;
        };
        if (!has_road_4nei(home, home_type)) {
            ++stats->home_no_road;
        }
        for (const Candidate& cnd : candidates) {
            if (!has_road_4nei(*cnd.b, *cnd.t)) {
                ++stats->goal_no_road;
            }
        }
    }
    const auto& tables = types.walk_tables();
    if (!tables) {
        if (stats != nullptr) {
            ++stats->incomplete;
        }
        return std::nullopt;
    }
    const std::vector<WalkPoint> starts = entrance_points(home, home_type);
    struct Goal {
        WalkPoint point;
        std::size_t candidate = 0;
    };
    std::vector<Goal> mapped_goals;
    std::vector<WalkPoint> goals;
    for (std::size_t k = 0; k < candidates.size(); ++k) {
        for (const WalkPoint& p : entrance_points(*candidates[k].b, *candidates[k].t)) {
            const auto duplicate = std::find_if(
                mapped_goals.begin(), mapped_goals.end(), [&](const Goal& g) { return g.point == p; });
            if (duplicate == mapped_goals.end()) {
                mapped_goals.push_back(Goal{p, k});
                goals.push_back(p);
            }
        }
    }
    WalkCells cells(*island, types, *tables, area.square(), 4, area);
    // The best score starts at 0 for every search (0x10003dbf).
    std::int64_t best_score = 0;
    std::optional<std::size_t> best;
    GoodsId best_goods = 0;
    bool reached = false; // a goal node was visited (T27 diagnostic)
    auto visit_cart = [&](std::size_t k) {
        // The callback 0x1005a900.
        const Building& b = *candidates[k].b;
        const BuildingType& t = *candidates[k].t;
        const std::int64_t cap = output_capacity(t); // type +0xc0, for both outputs
        bool selected = false;
        for (std::size_t i = 0; i < 2; ++i) {
            const GoodsId goods = listed_goods(t.output_goods, i);
            if (i == 1 && goods == 0) {
                break; // 0x1005ab17-0x1005ab29: no second output
            }
            if (goods >= c->stock.stock.size() || c->stock.capacity <= c->stock.stock[goods]) {
                continue; // the city has no room for it (city +0x78 > +0x7c(g))
            }
            const std::int64_t avail = available(b, t, goods, c);
            std::int64_t score = 0;
            if (raw_goods(goods)) {
                const std::int64_t q = 3 * cap / 4;
                if (cap - q == 0) {
                    continue; // the original divides by zero here (idiv): skipped
                }
                score = (avail - q) * 128 / (cap - q);
            } else {
                if (cap == 0) {
                    continue; // idiv by 0 in the original: skipped
                }
                score = avail * 128 / cap;
                if (avail < kOneTon) {
                    continue; // 0x1005aacc
                }
            }
            if (best_score >= score) {
                continue; // searcher +0x8c < score is needed
            }
            best_score = score;
            best = k;
            best_goods = goods;
            selected = true;
            if (score >= kStopScore &&
                (!raw_goods(goods) || 2 * cap >= std::int64_t{c->stock.stock[goods]})) {
                return SearchDecision{GoalAction::Stop, true};
            }
        }
        return SearchDecision{GoalAction::Expand, selected};
    };
    auto visit_carrier = [&](std::size_t k) {
        // The callback 0x1005a400.
        GoodsId goods = 0;
        const std::int64_t v =
            workshop_offer(world, types, home, home_type, *candidates[k].b, c, goods);
        if (best_score >= v) {
            return SearchDecision{};
        }
        best_score = v;
        best = k;
        best_goods = goods;
        // Stop when the source holds 3/4 of the carrier's capacity (0x1005a6a7, the
        // context's +4; 0x1005a4d9).
        const std::int64_t q = std::int64_t{capacity_88} * 3 / 4;
        const bool keep_going = available(*candidates[k].b, *candidates[k].t, goods, c) < q;
        return SearchDecision{keep_going ? GoalAction::Expand : GoalAction::Stop, true};
    };
    auto visit = [&](const SearchVisit& v) {
        reached = true;
        const auto goal = std::find_if(mapped_goals.begin(), mapped_goals.end(),
                                       [&](const Goal& g) { return g.point == v.point; });
        if (goal == mapped_goals.end()) {
            return SearchDecision{};
        }
        const std::size_t k = goal->candidate;
        if (candidates[k].scored) {
            return SearchDecision{}; // its further entrances score the same (strict `<`)
        }
        candidates[k].scored = true;
        return cart ? visit_cart(k) : visit_carrier(k);
    };
    const SearchResult found = search(cells, *tables, layer, starts, goals, visit);
    if (stats != nullptr) {
        stats->start_outside_rect += found.start_outside_rect;
    }
    if (!found.complete) {
        // Deviation: the scenario adapter discarded a retained tile-only object's
        // placement rotation/offsets (T33.2), so an exact cell cannot be built. Do not
        // choose a route from incomplete geometry.
        if (stats != nullptr) {
            ++stats->incomplete;
        }
        return std::nullopt;
    }
    if (!best) {
        if (stats != nullptr) {
            ++(reached ? stats->low_score : stats->no_goal_reached);
        }
        return std::nullopt;
    }
    SourceChoice choice;
    choice.building = candidates[*best].b->id;
    choice.goods = best_goods;
    choice.path = found.route;
    if (choice.path.empty()) {
        if (stats != nullptr) {
            ++stats->unreached;
        }
        return std::nullopt;
    }
    if (stats != nullptr) {
        ++stats->found;
    }
    return choice;
}

std::optional<std::vector<TileXY>> path_home(const Island& island, const TypeRegistry& types,
                                             const Building& home, const BuildingType& home_type,
                                             FixedPos from, std::uint32_t layer) {
    const SearchArea area = search_area(home, home_type);
    const auto& tables = types.walk_tables();
    if (!tables) {
        return std::nullopt;
    }
    WalkCells cells(island, types, *tables, area.square(), 4, area);
    const std::array starts{walk_point(from)};
    const std::vector<WalkPoint> goals = entrance_points(home, home_type);
    const SearchResult found = search(cells, *tables, layer, starts, goals,
                                      [](const SearchVisit&) {
                                          return SearchDecision{GoalAction::Stop, true};
                                      });
    if (!found.complete || !found.selected || found.route.empty()) {
        return std::nullopt;
    }
    return found.route;
}

namespace {

/// The route of a walk: the figure's spot, then the centres of the path's tiles after
/// the first (the path starts on the figure's own or its home's tile).
void set_route(Figure& fig, const std::vector<TileXY>& path) {
    TransportState& tr = fig.transport;
    tr.route.clear();
    tr.route.push_back(fig.pos);
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i == 0 && path.size() > 1) {
            continue; // the start tile: the walk starts from the figure's spot
        }
        tr.route.push_back(centre(path[i]));
    }
    tr.next = 1;
    tr.along = 0;
}

/// Walks `ticks` along the route at `speed` (2^-24 tile per tick); the ticks left after
/// the arrival are returned (the original carries the rest of its time to the next
/// action, 0x100347b1-0x10034808; here at a tick's resolution, OQ-162), nullopt when
/// the walk goes on.
std::optional<std::uint64_t> walk(Figure& fig, std::uint64_t ticks, std::uint32_t speed) {
    TransportState& tr = fig.transport;
    if (tr.route.empty() || tr.next >= tr.route.size()) {
        tr.route.clear();
        tr.next = 0;
        tr.along = 0;
        return ticks;
    }
    if (speed == 0) {
        return std::nullopt; // a figure type without a speed never arrives
    }
    std::uint64_t dist = ticks * speed;
    while (tr.next < tr.route.size()) {
        const FixedPos a = tr.route[tr.next - 1];
        const FixedPos b = tr.route[tr.next];
        const std::uint64_t seg = segment_length_q24(a, b);
        const std::uint64_t left = seg > tr.along ? seg - tr.along : 0;
        if (dist < left) {
            tr.along = static_cast<std::uint32_t>(tr.along + dist);
            const std::int64_t num = tr.along;
            const auto den = static_cast<std::int64_t>(seg);
            fig.pos = FixedPos{clamp32(a.x + (std::int64_t{b.x} - a.x) * num / den),
                               clamp32(a.y + (std::int64_t{b.y} - a.y) * num / den)};
            return std::nullopt;
        }
        dist -= left;
        tr.along = 0;
        fig.pos = b;
        ++tr.next;
    }
    tr.route.clear();
    tr.next = 0;
    tr.along = 0;
    return dist / speed;
}

struct Job {
    const TickContext& ctx;
    SerialWorld& world;
    Figure& fig;
    const FigureType* ft;
    std::uint32_t layer;
    const Building* home;
    const BuildingType* home_type;
    std::optional<CityRef> home_city;
};

/// Step C's start (0x10057ae0): inside the home's footprint the step is done at once;
/// else a walk home, or no path: the job ends (the cargo stays with the figure).
void start_home(Job& j) {
    TransportState& tr = j.fig.transport;
    tr.route.clear();
    tr.next = 0;
    tr.along = 0;
    if (j.home == nullptr || j.home_type == nullptr) {
        tr.step = TransportStep::Done;
        return;
    }
    const TileXY at{tile_of(j.fig.pos.x), tile_of(j.fig.pos.y)};
    tr.step = TransportStep::ToHome;
    if (in_footprint(*j.home, *j.home_type, at.x, at.y)) {
        return; // no walk: the arrival follows in the same call
    }
    const Island* island = j.world.island(island_of(j.home->id));
    const auto path = island != nullptr ? path_home(*island, j.ctx.types, *j.home, *j.home_type,
                                                    j.fig.pos, j.layer)
                                        : std::nullopt;
    if (!path) {
        tr.step = TransportStep::Done; // figure slot +0xcc found no path (0x10057bb1)
        return;
    }
    set_route(j.fig, *path);
}

/// The source's city (for a market source: its stock; for a producer: unused).
const City* source_city(const SerialWorld& world, const Building& b) {
    const auto c = city_of(world, b.id);
    return c ? world.city(*c) : nullptr;
}

/// Step A's start (cart 0x1005ad10, carrier 0x1005a530).
void start_source(Job& j) {
    TransportState& tr = j.fig.transport;
    if (j.home == nullptr || j.home_type == nullptr || !j.home_city) {
        tr.step = TransportStep::Done;
        return;
    }
    const std::uint32_t capacity = j.ft != nullptr ? j.ft->capacity_88 : 0;
    const auto choice = choose_source(j.world, j.ctx.types, j.fig.job, *j.home, *j.home_type,
                                      *j.home_city, capacity, j.layer, j.ctx.transport_stats);
    const Building* target = choice ? j.world.find_building(choice->building) : nullptr;
    const BuildingType* target_type = target != nullptr ? j.ctx.types.find(target->type) : nullptr;
    if (target == nullptr || target_type == nullptr) {
        // No candidate, no score above the start or no path: the job goes to step C
        // (0x1005d380: step type 9 -> the tail node).
        start_home(j);
        return;
    }
    // The reservation (0x1005aee8 / 0x1005a739): building +0xb4(g, capacity) =
    // min(capacity, available) for a producer (0x10068460); a market reserves nothing
    // (0x100a3930 answers 0).
    std::uint32_t reserved = 0;
    if (!std::holds_alternative<Market>(target->payload)) {
        const std::int64_t avail = available(*target, *target_type, choice->goods, nullptr);
        reserved = static_cast<std::uint32_t>(std::clamp<std::int64_t>(avail, 0, capacity));
        if (reserved > 0) {
            j.world.fx.store_deltas.push_back(StoreDelta{
                target->id, StoreDelta::Store::Reserved,
                store_line(*target, *target_type, choice->goods), std::int64_t{reserved}});
        }
    }
    j.fig.cargo_goods = choice->goods;
    tr.reserved_88 = reserved;
    tr.target = target->id;
    set_route(j.fig, choice->path);
    tr.step = TransportStep::ToSource;
}

/// Step B's arrival (0x100551f0): un-reserve, then take up to the capacity of what is
/// not reserved by others; the cargo's amount grows by it (cargo slot +0xc adds).
void load(Job& j) {
    TransportState& tr = j.fig.transport;
    const std::uint32_t capacity = j.ft != nullptr ? j.ft->capacity_88 : 0;
    const Building* target = j.world.find_building(tr.target);
    const BuildingType* target_type = target != nullptr ? j.ctx.types.find(target->type) : nullptr;
    std::uint32_t take = 0;
    if (target != nullptr && target_type != nullptr) {
        if (std::holds_alternative<Market>(target->payload)) {
            // 0x1006b7f0: the city's remove, clipped at the stock.
            const City* c = source_city(j.world, *target);
            const auto cref = city_of(j.world, target->id);
            if (c != nullptr && cref && j.fig.cargo_goods < c->stock.stock.size()) {
                take = std::min(capacity, c->stock.stock[j.fig.cargo_goods]);
                if (take > 0) {
                    j.world.fx.goods_moves.push_back(
                        GoodsMove{*cref, j.fig.cargo_goods, -std::int64_t{take}});
                }
            }
        } else if (has_output_store(target->payload)) {
            const std::uint32_t line = store_line(*target, *target_type, j.fig.cargo_goods);
            const std::uint32_t res = reserved_of(*target, line);
            const std::uint32_t unres = std::min(res, tr.reserved_88); // 0x10068490
            if (unres > 0) {
                j.world.fx.store_deltas.push_back(StoreDelta{
                    target->id, StoreDelta::Store::Reserved, line, -std::int64_t{unres}});
            }
            const std::int64_t free =
                std::int64_t{out_of(*target, line)} - (std::int64_t{res} - unres); // 0x10068430
            take = static_cast<std::uint32_t>(std::clamp<std::int64_t>(free, 0, capacity));
            if (take > 0) {
                j.world.fx.store_deltas.push_back(
                    StoreDelta{target->id, StoreDelta::Store::Output, line, -std::int64_t{take}});
            }
        }
    }
    j.fig.cargo_88 += take;
    tr.reserved_88 = 0; // cargo +0x20 takes the reservation
}

/// Step C's arrival (0x10057bd0): with a home and goods, home +0xc0(g, amount): a market
/// adds to its city (clipped at the capacity; what the clip leaves is lost with the
/// figure), a workshop to its input store (0x100683f0, no clip). The cargo is not
/// cleared (the figure ends).
void unload(Job& j) {
    if (j.home == nullptr || j.home_type == nullptr || j.fig.cargo_goods == 0) {
        return;
    }
    if (std::holds_alternative<Market>(j.home->payload)) {
        if (j.home_city) {
            j.world.fx.goods_moves.push_back(
                GoodsMove{*j.home_city, j.fig.cargo_goods, std::int64_t{j.fig.cargo_88}});
        }
        return;
    }
    if (std::holds_alternative<Workshop>(j.home->payload) && j.fig.cargo_88 > 0) {
        const GoodsId g = j.fig.cargo_goods;
        const std::uint32_t index =
            j.home_type->input_goods.size() > 1 && g == j.home_type->input_goods[1] ? 1U : 0U;
        j.world.fx.store_deltas.push_back(
            StoreDelta{j.home->id, StoreDelta::Store::Input, index, std::int64_t{j.fig.cargo_88}});
    }
}

/// Step A's abort (0x1005a760): the building on the step's tile un-reserves.
void abort_source(Job& j) {
    TransportState& tr = j.fig.transport;
    const Building* target = j.world.find_building(tr.target);
    const BuildingType* target_type = target != nullptr ? j.ctx.types.find(target->type) : nullptr;
    if (target != nullptr && target_type != nullptr && tr.reserved_88 > 0 &&
        has_output_store(target->payload)) {
        j.world.fx.store_deltas.push_back(StoreDelta{
            target->id, StoreDelta::Store::Reserved,
            store_line(*target, *target_type, j.fig.cargo_goods),
            -std::int64_t{std::min(
                tr.reserved_88,
                reserved_of(*target, store_line(*target, *target_type, j.fig.cargo_goods)))}});
    }
    tr.reserved_88 = 0;
}

} // namespace

bool transport_tick(const TickContext& ctx, SerialWorld& world, Figure& fig) {
    // A figure whose type the registry does not know (a registry built without
    // FigurenHeader.dat) has no speed, capacity or layer: the default behaviour stays
    // (own design; every figure of the original has a type).
    const FigureType* ft = ctx.types.find_figure_type(fig.kind);
    if (ft == nullptr) {
        return false;
    }
    const std::uint32_t t = ctx.t;
    if (fig.expiry_stamp.has_value()) {
        // Ended, waiting for phase C (it has no home any more: validate_figures).
        fig.stamp = static_cast<std::uint16_t>(t);
        return true;
    }
    const std::uint32_t speed = ft->speed_q24;
    TransportState& tr = fig.transport;
    std::uint64_t ticks = 0;
    bool timed = false;
    if (tr.step == TransportStep::ToSource || tr.step == TransportStep::ToHome) {
        // A walk that does not end in this call reads nothing of the world (the home
        // is looked up below, when a walk ends or a step starts).
        ticks = static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - fig.stamp);
        fig.stamp = static_cast<std::uint16_t>(t);
        timed = true;
        const auto rest = walk(fig, ticks, speed);
        if (!rest) {
            return true;
        }
        ticks = *rest; // the route is used up: the walk below returns these ticks
    }
    const Building* home = world.find_building(fig.home);
    const BuildingType* home_type = home != nullptr ? ctx.types.find(home->type) : nullptr;
    if (fig.job == kJobWorkshopCarrier && home != nullptr &&
        !std::holds_alternative<Workshop>(home->payload)) {
        return false; // the weapons workshop's carrier: stage 2 (it never leaves Start)
    }
    if (!timed) {
        ticks = static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - fig.stamp);
        fig.stamp = static_cast<std::uint16_t>(t);
    }
    Job j{ctx,
          world,
          fig,
          ft,
          ft->layer,
          home,
          home_type,
          home != nullptr ? city_of(world, home->id) : std::nullopt};
    // Every pass waits for time or moves to a later step (Start, ToSource, Load, ToHome,
    // Done), so a call ends after at most five passes.
    for (int pass = 0; pass < 8; ++pass) {
        switch (tr.step) {
        case TransportStep::Start:
            start_source(j);
            break;
        case TransportStep::ToSource: {
            const auto rest = walk(fig, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            if (home == nullptr) {
                // An arrival without a home aborts the job (0x1005d3c0 -> +0x38).
                abort_source(j);
                tr.step = TransportStep::Done;
                break;
            }
            // Step A's arrival does nothing (0x1005f710); step B's start waits (0x100551b0).
            tr.wait = ft != nullptr ? ft->load_ticks : 0;
            tr.step = TransportStep::Load;
            break;
        }
        case TransportStep::Load:
            if (ticks < tr.wait) {
                tr.wait = static_cast<std::uint32_t>(tr.wait - ticks);
                return true;
            }
            ticks -= tr.wait;
            tr.wait = 0;
            load(j);
            start_home(j);
            break;
        case TransportStep::ToHome: {
            const auto rest = walk(fig, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            unload(j);
            tr.step = TransportStep::Done;
            break;
        }
        case TransportStep::Done:
            // No step left (0x10034380 deletes the job): the end path 0x1003af00 inside the
            // home is a walk of 0 (the release and the Remove at t; outside it the original
            // walks 15.0 first, which end_figure does not model).
            world.end_figure(fig, t);
            return true;
        }
    }
    return true;
}

} // namespace anno::sim
