#include <anno/sim/route_cache.hpp>
#include <anno/sim/route_world.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

struct RouteCache::Impl {
    explicit Impl(const World& w) : source(w), index(source, {}) {}
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
RouteCache::RouteCache(RouteCache&&) noexcept {}
RouteCache& RouteCache::operator=(RouteCache&&) noexcept {
    impl_.reset();
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
