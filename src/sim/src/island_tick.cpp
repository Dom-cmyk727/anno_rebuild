// The island phase of World::advance (docs/sim/state.md §5.4–§5.8; stages S7 and S13):
// the island tick with its two sweeps, the object manager, the city ring, the city tick,
// the building scheduler, the 300 s event and the drought.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

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
    Drought d;
    d.x = x;
    d.y = y;
    d.target_ring = radius / cfg.chunk_edge;
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
            // Tile coordinates inside the world are non-negative (validate_islands);
            // floor to the chunk edge.
            const auto bx = static_cast<std::uint32_t>(b->x);
            const auto by = static_cast<std::uint32_t>(b->y);
            start_drought_at(ctx.cfg, island, bx - bx % ctx.cfg.chunk_edge,
                             by - by % ctx.cfg.chunk_edge, ctx.cfg.drought_radius, ctx.t);
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
    // drought_tick (D: 0x1008aeb0) is stage S13.
    island.sweep.stamp = static_cast<std::uint16_t>(t);
}

} // namespace anno::sim
