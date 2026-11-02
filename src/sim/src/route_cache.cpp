#include <anno/sim/route_cache.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

struct RouteCache::Impl {
    explicit Impl(const World& w) : source(w, ship_route_terrain_options()), index(source, {}) {}
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
    impl_.reset();
}

const CoastIndex& RouteCache::get(const World& world) {
    if (impl_ == nullptr) {
        impl_ = std::make_unique<Impl>(world);
    }
    return impl_->index;
}

} // namespace anno::sim
