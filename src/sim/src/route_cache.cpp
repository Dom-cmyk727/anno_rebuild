#include <algorithm>
#include <vector>

#include <anno/sim/route_cache.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

/// What of an island the index assumes immutable: its address (the source holds pointers), its
/// rectangle and its terrain (shared, const). A difference means islands were replaced wholesale.
struct IslandPrint {
    const Island* island = nullptr;
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
    const Terrain* terrain = nullptr;
    std::uint64_t digest = 0;
    friend bool operator==(const IslandPrint&, const IslandPrint&) = default;
};

std::vector<IslandPrint> prints_of(const World& world) {
    std::vector<IslandPrint> out;
    out.reserve(world.islands().size());
    for (const auto& slot : world.islands()) {
        IslandPrint p;
        if (slot.has_value()) {
            p.island = &*slot;
            p.x0 = slot->tiles.x0();
            p.y0 = slot->tiles.y0();
            p.w = slot->tiles.width();
            p.h = slot->tiles.height();
            p.terrain = slot->terrain.get();
            p.digest = p.terrain != nullptr ? p.terrain->digest : 0;
        }
        out.push_back(p);
    }
    return out;
}

} // namespace

struct RouteCache::Impl {
    explicit Impl(const World& w)
        : prints(prints_of(w)), source(w, ship_route_terrain_options()), index(source, {}) {}
    std::vector<IslandPrint> prints;
    WorldTileSource source;
    CoastIndex index;
};

RouteCache::RouteCache() = default;
RouteCache::~RouteCache() = default;
RouteCache::RouteCache(const RouteCache&) {}
RouteCache& RouteCache::operator=(const RouteCache&) {
    impl_.reset();
    return *this;
}
RouteCache::RouteCache(RouteCache&& other) noexcept {
    other.impl_.reset(); // the source points into the old owner
}
RouteCache& RouteCache::operator=(RouteCache&& other) noexcept {
    impl_.reset();
    other.impl_.reset();
    return *this;
}

void RouteCache::clear() {
    synced_ = false;
    impl_.reset();
}

void RouteCache::build(World& world) {
    impl_ = std::make_unique<Impl>(world);
    ++work.index_builds;
    // The index was built from the current tiles: whatever was pending is included.
    for (auto& slot : world.islands_) {
        if (slot.has_value()) {
            slot->tiles.clear_route_dirty();
        }
    }
}

void RouteCache::sync_once(World& world, std::uint64_t tick) {
    if (synced_ && synced_tick_ == tick && impl_ != nullptr) {
        return;
    }
    sync(world);
    synced_ = true;
    synced_tick_ = tick;
}

void RouteCache::sync(World& world) {
    if (impl_ == nullptr || impl_->prints != prints_of(world)) {
        build(world);
        return;
    }
    for (auto& slot : world.islands_) {
        if (!slot.has_value() || !slot->tiles.route_dirty().any()) {
            continue;
        }
        const RouteDirty& d = slot->tiles.route_dirty();
        const TileLayer& t = slot->tiles;
        // Tiles, half-open, world coordinates; one coarse cell (8 tiles) of halo on every side as a
        // safety margin: the connectivity mask of a cell depends on that cell's tiles alone and the
        // shore band on the eight neighbours' records, which rebuild_rect covers with its own type
        // halo (docs/sim/ship-navigation.md 12.13, halo evidence).
        const std::int64_t kHalo = 8; // the coarse cell edge of the index (route.hpp), not a limit
        std::int64_t x0 = d.all() ? t.x0() : d.x0();
        std::int64_t y0 = d.all() ? t.y0() : d.y0();
        std::int64_t x1 = d.all() ? std::int64_t{t.x0()} + t.width() : d.x1();
        std::int64_t y1 = d.all() ? std::int64_t{t.y0()} + t.height() : d.y1();
        x0 = std::max<std::int64_t>(x0 - kHalo, 0);
        y0 = std::max<std::int64_t>(y0 - kHalo, 0);
        x1 = std::min<std::int64_t>(x1 + kHalo, impl_->index.width_tiles());
        y1 = std::min<std::int64_t>(y1 + kHalo, impl_->index.height_tiles());
        impl_->index.rebuild_rect(
            CoastRect{static_cast<std::int32_t>(x0), static_cast<std::int32_t>(y0),
                      static_cast<std::int32_t>(x1), static_cast<std::int32_t>(y1)},
            {});
        ++work.index_rect_rebuilds;
        // The cells whose mask is rebuilt (the type halo of one more cell is rebuild_rect's own).
        if (x1 > x0 && y1 > y0) { // a dirty tile beyond the index counts no cells
            work.index_cells_rebuilt += static_cast<std::uint64_t>((x1 - 1) / 8 - x0 / 8 + 1) *
                                        static_cast<std::uint64_t>((y1 - 1) / 8 - y0 / 8 + 1);
        }
        slot->tiles.clear_route_dirty();
    }
}

const CoastIndex& RouteCache::get(const World& world) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>(world);
    }
    return impl_->index;
}

} // namespace anno::sim
