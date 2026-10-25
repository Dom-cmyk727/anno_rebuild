// The island phase of World::advance (docs/sim/state.md §5.4–§5.8; stages S7 and S13):
// the island tick with its two sweeps, the object manager, the city ring, the city tick,
// the building scheduler, the 300 s event and the drought.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <anno/sim/clock.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/island_tick.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/scheduler.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

void advance_sweep_cursor(std::uint32_t& x, std::uint32_t& y, std::uint32_t width,
                          std::uint32_t height, std::uint32_t chunk_edge) {
    // A3: x resets on every row wrap; y wraps at the bottom of the island. u64
    // sums, so a hostile edge cannot wrap the u32.
    const std::uint64_t nx = std::uint64_t{x} + chunk_edge;
    if (nx >= width) {
        x = 0;
        const std::uint64_t ny = std::uint64_t{y} + chunk_edge;
        y = ny >= height ? 0 : static_cast<std::uint32_t>(ny);
    } else {
        x = static_cast<std::uint32_t>(nx);
    }
}

void grow_chunk(const TickContext& ctx, Island& island, std::uint32_t x, std::uint32_t y) {
    // D 0x1008b5f0 (arguments x, y, t): the original walks a fixed 8 x 8 block from
    // tiles + 8 (w y + x) with no bounds check; the edge chunks of an island whose sides
    // are not multiples of the edge are clipped to the in-bounds tiles here (A14, F20).
    TileLayer& tiles = island.tiles;
    const std::uint64_t w = tiles.width();
    const std::uint64_t x_end = std::min<std::uint64_t>(std::uint64_t{x} + ctx.cfg.chunk_edge, w);
    const std::uint64_t y_end =
        std::min<std::uint64_t>(std::uint64_t{y} + ctx.cfg.chunk_edge, tiles.height());
    for (std::uint64_t row = y; row < y_end; ++row) {
        for (std::uint64_t col = x; col < x_end; ++col) {
            const std::uint64_t idx = row * w + col;
            const std::uint32_t object = tiles.object(idx);
            // The type through game slot +0x70; a tile without one, or whose type has no
            // growth period, is skipped (grow() returns at once for P / 2 = 0).
            const BuildingType* type = ctx.types.find_growing(object);
            if (type == nullptr) {
                continue;
            }
            const std::uint8_t phase = tiles.phase(idx);
            GrowthTile g{object, phase};
            static_cast<void>(grow(g, type->prod, ctx.t));
            // Written back through the setters, which mark the chunk dirty.
            if (g.object != object) {
                tiles.set_object(idx, g.object);
            }
            if (g.phase != phase) {
                tiles.set_phase(idx, g.phase);
            }
        }
    }
}

void object_manager_tick(const TickContext& ctx, Island& island, Effects& fx) {
    // Only the stamp arithmetic is D (0x10095d40); the traversal is the stand-in of
    // A10: the objects are the deposits in index order.
    const std::uint64_t n = island.deposits.size();
    const std::uint64_t count = visit_count(ctx.t, island.objmgr.stamp, n, ctx.cfg.objmgr_div);
    const Seams::IslandPass body = ctx.seams.object_manager.fn;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (body != nullptr) {
            body(ctx.seams.object_manager.ctx, ctx, island, fx);
        }
        island.objmgr.cursor = static_cast<std::uint32_t>((island.objmgr.cursor + 1U) % n);
    }
    island.objmgr.stamp = static_cast<std::uint16_t>(ctx.t);
}

void building_scheduler(const TickContext& ctx, Island& island, City& city, Effects& fx) {
    run_scheduled_lists(
        city.index.lists, ctx.cfg.layout.schedule, city.index.stamp, ctx.t, ctx.cfg.building_div,
        [&](ListId /*list*/, Building& b) { tick_building(ctx, island, city, b, fx); });
}

void city_tick(const TickContext& ctx, Island& island, City& city, Effects& fx) {
    building_scheduler(ctx, island, city, fx);
    for (const Seam<Seams::CityPass>& pass : ctx.seams.city_pass) {
        if (pass.fn != nullptr) {
            pass.fn(pass.ctx, ctx, island, city, fx);
        }
    }
    city.tick_stamp = static_cast<std::uint16_t>(ctx.t);
}

void city_ring_tick(const TickContext& ctx, Island& island, Effects& fx) {
    const std::uint64_t n = island.cities.size();
    const std::uint64_t count = visit_count(ctx.t, island.ring.stamp, n, ctx.cfg.city_ring_div);
    // A cursor at or beyond n is an inconsistent state (A8, validate_islands): nothing
    // is visited then, and the stamp is still written.
    if (island.ring.cursor < n) {
        for (std::uint64_t i = 0; i < count; ++i) {
            std::optional<City>& slot = island.cities[island.ring.cursor];
            if (slot.has_value()) {
                city_tick(ctx, island, *slot, fx);
            }
            island.ring.cursor = static_cast<std::uint32_t>((island.ring.cursor + 1U) % n);
        }
    }
    island.ring.stamp = static_cast<std::uint16_t>(ctx.t);
}

void start_drought_at(const SimConfig& cfg, Island& island, std::uint32_t x, std::uint32_t y,
                      std::uint32_t radius, std::uint32_t t) {
    // D 0x10087280: the position floored to the chunk edge (0x100872b8..0x100872e5, a C
    // remainder, = floor for the non-negative coordinates), the target ring radius / edge
    // (0x100872e9..0x100872fc), state 1, the stamp = world property 4, the tick.
    Drought d;
    d.x = x - x % cfg.chunk_edge;
    d.y = y - y % cfg.chunk_edge;
    // At most kMaxDroughtRing: the ring is an i32 that runs up to the target. The 300 s
    // event's radius is bounded by SimConfig::validate; a scripted caller's is cut here.
    d.target_ring = std::min(radius / cfg.chunk_edge, kMaxDroughtRing);
    d.ring = 0;
    d.city_mask.assign(static_cast<std::size_t>(drought_mask_words(cfg.city_slots)), 0U);
    d.phase = 0;
    d.step = 0;
    d.state = 1;
    d.counter = 0;
    d.stamp = static_cast<std::uint16_t>(t);
    island.drought = std::move(d);
}

bool start_drought(const TickContext& ctx, Island& island) {
    if (island.drought.has_value()) {
        return false;
    }
    const ListId market = ctx.cfg.layout.roles.market_list;
    for (const std::optional<City>& city : island.cities) {
        if (!city.has_value() || market >= city->index.lists.size()) {
            continue;
        }
        for (const std::optional<Building>& b : city->index.lists[market].slots) {
            if (!b.has_value()) {
                continue;
            }
            // The building's position as it is (slots +0x54 / +0x58); the constructor
            // floors it. Tile coordinates inside the world are non-negative
            // (validate_islands).
            start_drought_at(ctx.cfg, island, static_cast<std::uint32_t>(b->x),
                             static_cast<std::uint32_t>(b->y), ctx.cfg.drought_radius, ctx.t);
            return true;
        }
    }
    return false;
}

void event_300s(const TickContext& ctx, Island& island, Effects& /*fx*/) {
    const std::uint32_t seconds = ctx.t / 500U;
    const auto gate = static_cast<std::uint16_t>(seconds - island.event300_stamp);
    if (gate < ctx.cfg.event300_gate_seconds) {
        return; // the stamp stays
    }
    const auto stamp = static_cast<std::uint16_t>(seconds);
    if (ctx.settings.drought_off()) {
        island.event300_stamp = stamp; // no draw at all
        return;
    }
    // EXACTLY ONE draw per gate per island, before every other test.
    const std::uint32_t r = ctx.draw.next(DrawSite::Event300);
    bool has_city = false;
    const std::uint64_t scan = std::min<std::uint64_t>(ctx.cfg.neutral_owner, island.cities.size());
    for (std::uint64_t k = 0; k < scan && !has_city; ++k) {
        has_city = island.cities[static_cast<std::size_t>(k)].has_value();
    }
    if (r % ctx.cfg.event300_modulus == 0 && island.climate != 0 && has_city) {
        static_cast<void>(start_drought(ctx, island));
    }
    island.event300_stamp = stamp;
}

DroughtStrip drought_strip(const SimConfig& cfg, const Island& island, const Drought& d) {
    // D 0x1008aefb..0x1008b014 (docs/sim/state.md §5.8). i64 throughout: the ring may be
    // up to 2^31 - 1 chunks (kMaxDroughtRing), the edge up to kWorldEdgeBound.
    DroughtStrip s;
    s.ring = d.ring;
    s.phase = d.phase;
    s.step = d.step;
    s.expanding = d.state == 1;
    const std::int64_t e = cfg.chunk_edge;
    const std::int64_t x = d.x;
    const std::int64_t y = d.y;
    // Ring 0: the centre chunk, for every phase, not clipped (0x1008af02..0x1008af20).
    std::int64_t x0 = x;
    std::int64_t x1 = x + e;
    std::int64_t y0 = y;
    std::int64_t y1 = y + e;
    if (d.ring != 0) {
        const std::int64_t r = e * std::int64_t{d.ring};
        switch (d.phase) {
        case 0: // top row (0x1008af46)
            x0 = x - r;
            x1 = x + r;
            y0 = y - r;
            y1 = y - r + e;
            break;
        case 1: // right column (0x1008af68)
            x0 = x + r;
            x1 = x + r + e;
            y0 = y - r;
            y1 = y + r;
            break;
        case 2: // bottom row (0x1008af84)
            x0 = x - r + e;
            x1 = x + r + e;
            y0 = y + r;
            y1 = y + r + e;
            break;
        case 3: // left column (0x1008afa9)
            x0 = x - r;
            x1 = x - r + e;
            y0 = y - r + e;
            y1 = y + r + e;
            break;
        default:
            // No rectangle in the original (0x1008af39 `ja` to an uninitialised local):
            // an empty strip (N, only with drought_phases above 4).
            return s;
        }
        // Clipped to the island (signed, 0x1008afd2..0x1008b014).
        const std::int64_t ix = island.tiles.x0();
        const std::int64_t iy = island.tiles.y0();
        x0 = std::max(x0, ix);
        x1 = std::min(x1, ix + std::int64_t{island.tiles.width()});
        y0 = std::max(y0, iy);
        y1 = std::min(y1, iy + std::int64_t{island.tiles.height()});
    }
    if (x0 >= x1 || y0 >= y1) {
        return s; // no tile (the loops at 0x1008b028 / 0x1008b036 do not run)
    }
    s.rect = TileRect{static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0),
                      static_cast<std::uint32_t>(x1 - x0), static_cast<std::uint32_t>(y1 - y0)};
    return s;
}

namespace {

/// The owner samples of an expanding strip at step 0 (D 0x1008b0c2..0x1008b195): per
/// chunk anchor (ax, ay) of the strip, the owners of the tiles (ax + a, ay + a), (ax + a,
/// ay + b), (ax + b, ay + a), (ax + b, ay + b) with a = e / 4 and b = 3 e / 4 (2 and 6
/// for the original 8) set their city slot bit in the mask. An owner equal to
/// neutral_owner (the original 15) or beyond the city slots gives no bit; a sample
/// outside the island is skipped (F36: the original reads it unchecked).
void sample_owners(const SimConfig& cfg, const Island& island, const DroughtStrip& s,
                   std::vector<std::uint32_t>& mask) {
    const std::int64_t e = cfg.chunk_edge;
    const std::int64_t a = e / 4;
    const std::int64_t b = 3 * e / 4;
    const std::int64_t offsets[4][2] = {{a, a}, {a, b}, {b, a}, {b, b}};
    const std::int64_t x_end = std::int64_t{s.rect.x0} + s.rect.w;
    const std::int64_t y_end = std::int64_t{s.rect.y0} + s.rect.h;
    for (std::int64_t ay = s.rect.y0; ay < y_end; ay += e) {
        for (std::int64_t ax = s.rect.x0; ax < x_end; ax += e) {
            for (const auto& o : offsets) {
                const std::int64_t sx = ax + o[0];
                const std::int64_t sy = ay + o[1];
                if (sx > std::numeric_limits<std::int32_t>::max() ||
                    sy > std::numeric_limits<std::int32_t>::max()) {
                    continue;
                }
                const Slot idx = island.tiles.index_of(static_cast<std::int32_t>(sx),
                                                       static_cast<std::int32_t>(sy));
                if (idx == kNoSlot) {
                    continue;
                }
                const std::uint32_t owner = island.tiles.owner(idx);
                if (owner == cfg.neutral_owner || owner >= cfg.city_slots) {
                    continue;
                }
                const std::size_t word = owner / 32U;
                if (word < mask.size()) {
                    mask[word] |= std::uint32_t{1} << (owner % 32U);
                }
            }
        }
    }
}

} // namespace

void drought_tick(const TickContext& ctx, Island& island, Effects& fx) {
    // D 0x1008aeb0 (docs/sim/state.md §5.8).
    if (!island.drought.has_value()) {
        return; // 0x1008aec2
    }
    const SimConfig& cfg = ctx.cfg;
    const auto t16 = static_cast<std::uint16_t>(ctx.t);
    {
        Drought& d = *island.drought;
        if (d.state == 0) {
            // The plateau (0x1008b58e): one count per 1 000 ticks; the catch-up gate moves
            // the stamp by the period, never to t.
            if (static_cast<std::uint16_t>(t16 - d.stamp) < cfg.drought_plateau_period) {
                return;
            }
            d.stamp = static_cast<std::uint16_t>(d.stamp + cfg.drought_plateau_period);
            d.counter = static_cast<std::uint16_t>(d.counter + 1U);
            if (d.counter >= cfg.drought_plateau_count) { // 0x1008b5be `jb`
                d.counter = 0;
                d.state = 0xff;
            }
            return;
        }
        // Expanding (1) or retreating (0xff): one strip per 50 ticks (0x1008aee2), the
        // stamp moved before the strip (0x1008aeec).
        if (static_cast<std::uint16_t>(t16 - d.stamp) < cfg.drought_strip_period) {
            return;
        }
        d.stamp = static_cast<std::uint16_t>(d.stamp + cfg.drought_strip_period);
    }
    const DroughtStrip strip = drought_strip(cfg, island, *island.drought);
    const Seam<Seams::DroughtTiles>& tiles = ctx.seams.drought_tiles;
    if (tiles.fn != nullptr) {
        tiles.fn(tiles.ctx, ctx, island, strip, fx);
        if (!island.drought.has_value()) {
            return; // a seam that removed the record ends the strip
        }
    }
    Drought& d = *island.drought;
    if (d.state == 1 && d.step == 0) {
        sample_owners(cfg, island, strip, d.city_mask);
    }
    // The step: one group of chunk_edge offsets per strip, drought_steps groups per phase
    // (`add 8`, `cmp 0x40; jb` 0x1008b4ed..0x1008b4fd).
    const std::uint32_t next_step = std::uint32_t{d.step} + cfg.chunk_edge;
    if (std::uint64_t{next_step} < std::uint64_t{cfg.drought_steps} * cfg.chunk_edge) {
        d.step = static_cast<std::uint8_t>(next_step);
        return;
    }
    d.step = 0;
    // The phase: the sides of the ring (`inc`, `cmp 4; jb` 0x1008b503..0x1008b51a).
    const std::uint32_t next_phase = std::uint32_t{d.phase} + 1U;
    if (next_phase < cfg.drought_phases) {
        d.phase = static_cast<std::uint8_t>(next_phase);
        return;
    }
    d.phase = 0;
    if (d.state == 1) {
        // 0x1008b530..0x1008b553: a ring past the target is the plateau at the target.
        if (std::int64_t{d.ring} + 1 > std::int64_t{d.target_ring}) {
            d.state = 0;
            d.ring = static_cast<std::int32_t>(d.target_ring);
        } else {
            d.ring += 1;
        }
    } else if (d.ring <= 0) {
        island.drought.reset(); // 0x1008b560..0x1008b57a: the ring went below 0
    } else {
        d.ring -= 1;
    }
}

void island_tick(const TickContext& ctx, Island& island, Effects& fx) {
    const std::uint32_t t = ctx.t;
    const TileLayer& tiles = island.tiles;
    const auto chunks = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(tiles.chunk_count(), std::numeric_limits<std::uint32_t>::max()));
    const auto d = static_cast<std::uint16_t>(t - island.sweep.stamp);
    // Growth sweep: the chunk at the cursor is visited (D 0x1008b5f0), then the cursor
    // moves on (D 0x1008b7c5..0x1008b7f5).
    const std::uint64_t grow_chunks = sweep_chunks(chunks, d, ctx.cfg.grow_period);
    for (std::uint64_t i = 0; i < grow_chunks; ++i) {
        grow_chunk(ctx, island, island.sweep.grow_x, island.sweep.grow_y);
        advance_sweep_cursor(island.sweep.grow_x, island.sweep.grow_y, tiles.width(),
                             tiles.height(), ctx.cfg.chunk_edge);
    }
    // Fauna sweep: the seam (D: island slot +0x194 = 0x10093480; PRNG, O-12).
    const std::uint64_t fauna_chunks = sweep_chunks(chunks, d, ctx.cfg.fauna_period);
    const Seam<Seams::ChunkPass>& wildlife = ctx.seams.wildlife_sweep;
    for (std::uint64_t i = 0; i < fauna_chunks; ++i) {
        if (wildlife.fn != nullptr) {
            wildlife.fn(wildlife.ctx, ctx, island, island.sweep.fauna_x, island.sweep.fauna_y, fx);
        }
        advance_sweep_cursor(island.sweep.fauna_x, island.sweep.fauna_y, tiles.width(),
                             tiles.height(), ctx.cfg.chunk_edge);
    }
    object_manager_tick(ctx, island, fx);
    // [island+0x30] message manager: I/O, not modelled (O-52).
    city_ring_tick(ctx, island, fx);
    event_300s(ctx, island, fx);
    drought_tick(ctx, island, fx); // D 0x1008b8ab
    island.sweep.stamp = static_cast<std::uint16_t>(t);
}

} // namespace anno::sim
