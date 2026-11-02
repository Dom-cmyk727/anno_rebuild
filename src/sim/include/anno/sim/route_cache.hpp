// The derived coast index of a World (MAP-NAV-2 package E, MAP-NAV-3 N3). Not state: never hashed,
// never saved. It is built on first use from the world's islands (route_world.hpp) and kept in step
// with them by sync(): every tile layer records the rectangle in which an object id, foot anchor or
// item height byte changed (TileLayer::route_dirty), and sync rebuilds only the coarse cells of
// that rectangle plus a one-cell halo. Terrain is immutable after load; a per-island fingerprint
// (address, rectangle, terrain) rebuilds the whole index if that ever stops being true. A copied or
// moved World starts with an empty cache: a copied index would point into the old World's islands.
#pragma once

#include <cstdint>
#include <memory>

namespace anno::sim {

class World;
class CoastIndex;

/// Deterministic work counters of the navigation adapters (diagnostic, derived, never hashed or
/// saved; copies start at zero). Locality tests assert on these, timings are only measurements.
struct NavWork {
    std::uint64_t admission_steps = 0;     // ticket-set entries examined by fair admission
    std::uint64_t stamp_cells = 0;         // ship-grid cells looked up by the stamp query
    std::uint64_t stamp_entries = 0;       // ship-grid entries examined (discarded ones included)
    std::uint64_t crew_visits = 0;         // descendants visited by the crew mirror
    std::uint64_t index_builds = 0;        // full coast index builds
    std::uint64_t index_rect_rebuilds = 0; // dirty rectangles applied incrementally
    friend bool operator==(const NavWork&, const NavWork&) = default;
};

class RouteCache {
  public:
    NavWork work;

    RouteCache();
    ~RouteCache();
    /// Copies and moves give an EMPTY cache (derived data is rebuilt, never shared).
    RouteCache(const RouteCache&);
    RouteCache& operator=(const RouteCache&);
    RouteCache(RouteCache&&) noexcept;
    RouteCache& operator=(RouteCache&&) noexcept;

    void clear();
    [[nodiscard]] bool built() const { return impl_ != nullptr; }
    /// Brings the index up to date with the islands of `world`: builds it when absent, rebuilds it
    /// when an island's fingerprint changed, else applies every pending tile-layer dirty rectangle
    /// (with a halo) and clears it. Call before planning; cheap when nothing changed.
    void sync(World& world);
    /// The index of `world`, built on first use. The reference stays valid until clear() or until
    /// the world is changed in a way that calls clear().
    [[nodiscard]] const CoastIndex& get(const World& world);

  private:
    struct Impl;
    void build(World& world);
    std::unique_ptr<Impl> impl_;
};

} // namespace anno::sim
