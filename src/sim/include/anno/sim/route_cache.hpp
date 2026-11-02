// The derived coast index of a World (MAP-NAV-2 package E). Not state: never hashed, never saved.
// It is built on first use from the world's islands (route_world.hpp) and dropped whenever the
// islands may have changed (a command that builds, demolishes or founds) or the World is copied or
// moved: a copied cache would point into the old World's islands.
#pragma once

#include <memory>

namespace anno::sim {

class World;
class CoastIndex;

class RouteCache {
  public:
    RouteCache();
    ~RouteCache();
    /// Copies and moves give an EMPTY cache (derived data is rebuilt, never shared).
    RouteCache(const RouteCache&);
    RouteCache& operator=(const RouteCache&);
    RouteCache(RouteCache&&) noexcept;
    RouteCache& operator=(RouteCache&&) noexcept;

    void clear();
    [[nodiscard]] bool built() const { return impl_ != nullptr; }
    /// The index of `world`, built on first use. The reference stays valid until clear() or until
    /// the world is changed in a way that calls clear().
    [[nodiscard]] const CoastIndex& get(const World& world);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace anno::sim
